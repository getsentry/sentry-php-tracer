#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "Zend/zend_observer.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_attributes.h"
#if PHP_VERSION_ID < 80300
#include "ext/standard/hrtime.h"
#define zend_hrtime_t php_hrtime_t
#define zend_hrtime php_hrtime_current
#endif
#include <stdint.h>

#include "sentry_arginfo.h"
#ifdef PHP_WIN32
#include "win32/time.h"
#else
#include <sys/time.h>
#endif

ZEND_BEGIN_MODULE_GLOBALS(sentry)
    // Functions that should be observed. Values are the metadata arrays per instrumented call.
    HashTable instrumented_functions;

    // Call state of currently executing functions
    HashTable active_calls;

    // User callback that is invoked when an observed call begins. The return value is stored
    // and passed to the end callback.
    zval start_callback;

    // User callback that is invoked when an observed call ends.
    zval end_callback;

    // True when currently in a callback. Used as reentry guard to prevent recursion when
    // observed calls are invoked in the callback.
    bool in_callback;
ZEND_END_MODULE_GLOBALS(sentry)

ZEND_DECLARE_MODULE_GLOBALS(sentry)

#define SENTRY_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(sentry, v)

#if defined(ZTS) && defined(COMPILE_DL_SENTRY)
ZEND_TSRMLS_CACHE_DEFINE();
#endif

/**
 * Holds the name of the \Sentry\Trace attribute that can be used to retrieve it
 * using zend_get_attribute.
 */
static zend_string *sentry_trace_attribute_lcname;

static bool sentry_array_is_list(const zend_array *array) {
#if PHP_VERSION_ID >= 80100
    return zend_array_is_list(array);
#else
    zend_ulong expected_key = 0;
    zend_ulong num_key;
    zend_string *str_key;

    ZEND_HASH_FOREACH_KEY(array, num_key, str_key) {
        if (str_key != NULL || num_key != expected_key++) {
            return false;
        }
    } ZEND_HASH_FOREACH_END();

    return true;
#endif
}

// ==== CALL STATE BEGIN ====

/**
 * Tracks the state per function/method call. We need this to calculate how long an invocation took
 * and to produce proper spans
 */
typedef struct {
    // the name of the function to be traced, never NULL
    zend_string *name;
    // the wall-clock start time as unix timestamp
    double start_time;
    // monotonic start time used to calculate durations
    zend_hrtime_t start_hrtime;
    zval user_state;
    zval metadata;
} sentry_call_state;

/**
 * Destructor for the call state struct
 */
static void sentry_call_state_dtor(zval *zv) {
    sentry_call_state *state = Z_PTR_P(zv);

    zend_string_release(state->name);
    zval_ptr_dtor(&state->user_state);
    zval_ptr_dtor(&state->metadata);

    efree(state);
}

// ==== CALL STATE END ====

// ==== ATTRIBUTE START ====
static zend_attribute *sentry_get_trace_attribute(zend_execute_data *execute_data) {
    const zend_function *func = execute_data->func;

    if (func->common.attributes == NULL) {
        return NULL;
    }

    zend_attribute *attribute = zend_get_attribute(
        func->common.attributes,
        sentry_trace_attribute_lcname
    );

    return attribute;
}

static bool sentry_has_trace_attribute(zend_execute_data *execute_data) {
    return sentry_get_trace_attribute(execute_data) != NULL;
}

static void sentry_clear_pending_exception(void) {
    zend_object *ex = EG(exception);

    if (ex != NULL) {
        EG(exception) = NULL;
        OBJ_RELEASE(ex);
    }
}

static bool sentry_is_attribute_arg(zend_string *name, zval *value) {
    return name != NULL
        && zend_string_equals_literal(name, "attributes")
        && Z_TYPE_P(value) == IS_ARRAY;
}

/**
 * Copies all elements from source to dest if they are using string keys.
 * If source is a list, it will not do anything.
 * Integer keys are ignored and will not be copied.
 */
static void sentry_merge_array(zval *dest, zval *source) {
    if (sentry_array_is_list(Z_ARRVAL_P(source))) {
        return;
    }
    zend_string *str_key;
    zval *value;

    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(source), str_key, value) {
        if (str_key != NULL) {
            zval copied_value;
            ZVAL_COPY(&copied_value, value);

            zend_hash_update(
                Z_ARRVAL_P(dest),
                str_key,
                &copied_value
            );
        }
    }
    ZEND_HASH_FOREACH_END();
}

static void sentry_add_named_metadata_arg(
    zval *metadata,
    zval *deferred_attributes,
    zend_string *name,
    zval *value
) {
    if (sentry_is_attribute_arg(name, value)) {
        if (!Z_ISUNDEF_P(deferred_attributes)) {
            zval_ptr_dtor(deferred_attributes);
        }

        ZVAL_COPY(deferred_attributes, value);
        return;
    }

    zval copied_value;
    ZVAL_COPY(&copied_value, value);

    zend_hash_update(
        Z_ARRVAL_P(metadata),
        name,
        &copied_value
    );
}

static void sentry_merge_deferred_attributes(
    zval *metadata,
    zval *deferred_attributes
) {
    if (Z_ISUNDEF_P(deferred_attributes)) {
        return;
    }

    sentry_merge_array(metadata, deferred_attributes);
    zval_ptr_dtor(deferred_attributes);
    ZVAL_UNDEF(deferred_attributes);
}

static void sentry_get_attribute_metadata(
    zend_execute_data *execute_data,
    zval *metadata
) {
    array_init(metadata);

    zend_attribute *attribute = sentry_get_trace_attribute(execute_data);
    if (attribute == NULL || attribute->argc == 0) {
        return;
    }

    // this gets populated if the "attribute" named argument exists.
    // We will store it here and merge it last so that it always overwrites existing keys.
    zval attribute_list;
    ZVAL_UNDEF(&attribute_list);

    for (uint32_t i = 0; i < attribute->argc; i++) {
        zval attribute_arg;
        ZVAL_UNDEF(&attribute_arg);

        const zend_result result = zend_get_attribute_value(&attribute_arg, attribute, i, execute_data->func->common.scope);
        // result can be unsuccessful if e.g. constants are references that do not exist.
        if (result != SUCCESS) {
            // clear exception if the attribute has an invalid value so that we don't crash
            // user code.
            sentry_clear_pending_exception();

            if (!Z_ISUNDEF(attribute_arg)) {
                zval_ptr_dtor(&attribute_arg);
            }

            continue;
        }

        zend_string *arg_name = attribute->args[i].name;

        if (arg_name != NULL) {
            sentry_add_named_metadata_arg(
                metadata,
                &attribute_list,
                arg_name,
                &attribute_arg
            );
        } else if (Z_TYPE(attribute_arg) == IS_ARRAY) {
            sentry_merge_array(metadata, &attribute_arg);
        }

        if (!Z_ISUNDEF(attribute_arg)) {
            zval_ptr_dtor(&attribute_arg);
        }

    }

    if (!Z_ISUNDEF(attribute_list)) {
        sentry_merge_array(metadata, &attribute_list);
        zval_ptr_dtor(&attribute_list);
    }
}


ZEND_METHOD(Sentry_Trace, __construct) {
    zval *args = NULL;
    uint32_t argc = 0;
    HashTable *named_args = NULL;

    ZEND_PARSE_PARAMETERS_START(0, -1)
        Z_PARAM_VARIADIC_WITH_NAMED(args, argc, named_args)
    ZEND_PARSE_PARAMETERS_END();
}

// ==== ATTRIBUTE END ======

// ===== EXCEPTION ISOLATION START ===

// Stores exception and opline information before invoking the start or end callback.
// We do that so that the callback runs without any interference from exceptions that might
// be set from code before.
typedef struct {
    zend_object *exception;
    zend_object *prev_exception;
    const zend_op *opline_before_exception;
    bool has_opline;
    const zend_op *opline;
} sentry_exception_state;

static void sentry_exception_isolation_start(sentry_exception_state *state) {
    state->exception = EG(exception);
    state->prev_exception = EG(prev_exception);
    state->opline_before_exception = EG(opline_before_exception);

    EG(exception) = NULL;
    EG(prev_exception) = NULL;
    EG(opline_before_exception) = NULL;

    const zend_execute_data *execute_data = EG(current_execute_data);
    state->has_opline = execute_data != NULL;
    state->opline = execute_data ? execute_data->opline : NULL;
}

static zend_object *sentry_exception_isolation_end(sentry_exception_state *state) {
    zend_object *suppressed = EG(exception);

    // exit() unwinds via a fake exception that must not be intercepted: leave it
    // pending so the engine keeps tearing down the stack, and abandon the saved
    // exception — nothing will ever catch it, so we release our references here.
    if (UNEXPECTED(suppressed && zend_is_unwind_exit(suppressed))) {
        if (state->exception != NULL) {
            OBJ_RELEASE(state->exception);
        }
        if (state->prev_exception != NULL) {
            OBJ_RELEASE(state->prev_exception);
        }
        return NULL;
    }

    // Detach the exception the callback itself may have thrown: it lives on in
    // `suppressed` and the caller owns and releases it. It must not stay in
    // EG(exception), where it would mask the original exception we are about to
    // restore.
    // We have to set it to NULL otherwise zend_clear_exception will invoke
    // the exception handler.
    EG(exception) = NULL;
    zend_clear_exception();

    EG(exception) = state->exception;
    EG(prev_exception) = state->prev_exception;
    EG(opline_before_exception) = state->opline_before_exception;

    zend_execute_data *execute_data = EG(current_execute_data);
    if (execute_data != NULL && state->has_opline) {
        execute_data->opline = state->opline;
    }

    return suppressed;
}

static void sentry_call_user_function_isolated(
    zval *callback,
    zval *retval,
    uint32_t param_count,
    zval *params
) {
    SENTRY_G(in_callback) = true;

    sentry_exception_state state;
    sentry_exception_isolation_start(&state);

    call_user_function(
        EG(function_table),
        NULL,
        callback,
        retval,
        param_count,
        params
    );

    zend_object *suppressed = sentry_exception_isolation_end(&state);
    if (suppressed != NULL) {
        OBJ_RELEASE(suppressed);
    }

    SENTRY_G(in_callback) = false;
}

// ===== EXCEPTION ISOLATION END ====

static zend_string *sentry_to_key(zend_string *name) {
    if (name == NULL) {
        return NULL;
    }
    return zend_string_tolower(name);
}

static zend_string *sentry_build_display_name(zend_string *class_name, zend_string *function_name) {
    if (function_name == NULL) {
        return NULL;
    }

    if (class_name == NULL) {
        return zend_string_copy(function_name);
    }
    return zend_strpprintf(0, "%s::%s", ZSTR_VAL(class_name), ZSTR_VAL(function_name));
}

static zend_string *sentry_build_key(zend_string *class_name, zend_string *function_name) {
    zend_string *name = sentry_build_display_name(class_name, function_name);
    if (name == NULL) {
        return NULL;
    }
    zend_string *key = sentry_to_key(name);
    zend_string_release(name);

    return key;
}

ZEND_FUNCTION(Sentry_instrument) {
    zend_string *class_name = NULL;
    zend_string *function_name;
    // zval metadata;
    // zval *extra_metadata = NULL;

    zval *metadata_args = NULL;
    uint32_t metadata_argc = 0;
    HashTable *named_metadata = NULL;

    ZEND_PARSE_PARAMETERS_START(2,-1)
        Z_PARAM_STR_OR_NULL(class_name)
        Z_PARAM_STR(function_name)
        Z_PARAM_VARIADIC_WITH_NAMED(metadata_args, metadata_argc, named_metadata)
    ZEND_PARSE_PARAMETERS_END();

    // If a subclass doesn't override a method from the parent, the scope will
    // remain of the parent. For example, if A defined method food and B extends A
    // without overriding, doing (new B())->foo() will show up as A::foo in the
    // extension. This means that declaring an instrumentation on B::foo will never
    // trigger. The code below changes the classname so that it will correctly work
    // for subclasses.
    if (class_name != NULL) {
        zend_class_entry *ce = zend_lookup_class(class_name);
        if (ce != NULL) {
            zend_string *lc_func = zend_string_tolower(function_name);
            zend_function *func = zend_hash_find_ptr(&ce->function_table, lc_func);
            if (func != NULL && func->common.scope != NULL) {
                class_name = func->common.scope->name;
            }
            zend_string_release(lc_func);
        }
    }

    zval metadata;
    array_init(&metadata);

    zval attribute_list;
    ZVAL_UNDEF(&attribute_list);

    for (uint32_t i = 0; i < metadata_argc; i++) {
        if (Z_TYPE(metadata_args[i]) == IS_ARRAY) {
            sentry_merge_array(&metadata, &metadata_args[i]);
        }
    }

    if (named_metadata != NULL) {
        zend_string *name;
        zval *value;

        ZEND_HASH_FOREACH_STR_KEY_VAL(named_metadata, name, value) {
            if (name != NULL) {
                sentry_add_named_metadata_arg(
                    &metadata,
                    &attribute_list,
                    name,
                    value
                );
            }
        }
        ZEND_HASH_FOREACH_END();
    }

    sentry_merge_deferred_attributes(&metadata, &attribute_list);

    zend_string *key = sentry_build_key(class_name, function_name);

    const zval* inserted = zend_hash_add(&SENTRY_G(instrumented_functions), key, &metadata);

    // If the element wasn't inserted we have to manually destroy the local value to prevent memory leaks
    if (inserted == NULL) {
        zval_ptr_dtor(&metadata);
    }

    zend_string_release(key);

    RETURN_BOOL(inserted != NULL);
}

ZEND_FUNCTION(Sentry_setEndCallback) {
    zval *callback;

    ZEND_PARSE_PARAMETERS_START(1,1)
        Z_PARAM_ZVAL(callback)
    ZEND_PARSE_PARAMETERS_END();

    if (!zend_is_callable(callback, 0, NULL)) {
        zend_argument_type_error(1, "must be a valid callback");
        RETURN_THROWS();
    }

    if (!Z_ISUNDEF(SENTRY_G(end_callback))) {
        zval_ptr_dtor(&SENTRY_G(end_callback));
    }

    ZVAL_COPY(&SENTRY_G(end_callback), callback);

    RETURN_TRUE;
}

ZEND_FUNCTION(Sentry_setStartCallback) {
    zval *callback;

    ZEND_PARSE_PARAMETERS_START(1,1)
        Z_PARAM_ZVAL(callback)
    ZEND_PARSE_PARAMETERS_END();

    if (!zend_is_callable(callback, 0, NULL)) {
        zend_argument_type_error(1, "must be a valid callback");
        RETURN_THROWS();
    }

    if (!Z_ISUNDEF(SENTRY_G(start_callback))) {
        zval_ptr_dtor(&SENTRY_G(start_callback));
    }

    ZVAL_COPY(&SENTRY_G(start_callback), callback);

    RETURN_TRUE;
}

static bool sentry_should_observe(zend_execute_data *execute_data) {
    zend_class_entry *caller_class = execute_data->func->common.scope;

    zend_string *key = sentry_build_key(
        caller_class == NULL ? NULL : caller_class->name,
        execute_data->func->common.function_name
    );

    if (key == NULL) {
        return false;
    }

    const bool explicitly_instrumented = zend_hash_exists(&SENTRY_G(instrumented_functions), key);

    zend_string_release(key);

    if (explicitly_instrumented) {
        return true;
    }

    return sentry_has_trace_attribute(execute_data);
}

static void sentry_observer_begin(zend_execute_data *execute_data) {
    if (SENTRY_G(in_callback)) {
        return;
    }

    zend_class_entry *caller_class = zend_get_called_scope(execute_data);

    zend_string *name = sentry_build_display_name(
        caller_class == NULL ? NULL : caller_class->name,
        execute_data->func->common.function_name
    );

    zend_string *key = sentry_build_key(
        execute_data->func->common.scope == NULL ? NULL : execute_data->func->common.scope->name,
        execute_data->func->common.function_name
    );
    zval *metadata = zend_hash_find(&SENTRY_G(instrumented_functions), key);
    zend_string_release(key);

    zval attribute_metadata;
    bool using_attribute_metadata = false;

    if (metadata == NULL) {
        sentry_get_attribute_metadata(execute_data, &attribute_metadata);
        metadata = &attribute_metadata;
        using_attribute_metadata = true;
    }

    zval retval;
    ZVAL_UNDEF(&retval);

    struct timeval tv;
    (void) gettimeofday(&tv, NULL);

    sentry_call_state *state = emalloc(sizeof(sentry_call_state));
    state->name = name;
    state->start_time = tv.tv_sec + tv.tv_usec / 1000000.0;
    state->start_hrtime = zend_hrtime();

    ZVAL_COPY(&state->metadata, metadata);

    if (!Z_ISUNDEF(SENTRY_G(start_callback))) {
        zval data;
        array_init(&data);

        add_assoc_str(&data, "name", zend_string_copy(name));
        add_assoc_double(&data, "start_time", state->start_time);

        zval metadata_zv;
        ZVAL_COPY(&metadata_zv, metadata);
        add_assoc_zval(&data, "metadata", &metadata_zv);

        zval params[1];
        ZVAL_COPY_VALUE(&params[0], &data);

        sentry_call_user_function_isolated(
            &SENTRY_G(start_callback),
            &retval,
            1,
            params
        );

        zval_ptr_dtor(&params[0]);
    }

    if (Z_ISUNDEF(retval)) {
        ZVAL_NULL(&retval);
    }
    state->user_state = retval;

    zval state_zv;
    ZVAL_PTR(&state_zv, state);

    zend_hash_index_update(&SENTRY_G(active_calls), (zend_ulong) (uintptr_t) execute_data, &state_zv);

    if (using_attribute_metadata) {
        zval_ptr_dtor(&attribute_metadata);
    }
}


static void sentry_observer_end(zend_execute_data *execute_data, zval *return_value) {
    zend_ulong hash_key = (zend_ulong) (uintptr_t) execute_data;

    zval *state_zv = zend_hash_index_find(&SENTRY_G(active_calls), hash_key);

    if (state_zv == NULL) {
        return;
    }

    sentry_call_state *state = Z_PTR_P(state_zv);

    zend_hrtime_t elapsed_ns = zend_hrtime() - state->start_hrtime;
    double duration = elapsed_ns / 1000000.0;
    double end_time = state->start_time + (duration / 1000.0);

    if (!Z_ISUNDEF(SENTRY_G(end_callback))) {
        zval event;
        zval retval;
        ZVAL_UNDEF(&retval);
        zval params[2];

        array_init(&event);

        add_assoc_str(&event, "name", zend_string_copy(state->name));
        add_assoc_double(&event, "start_time", state->start_time);
        add_assoc_double(&event, "end_time", end_time);
        add_assoc_double(&event, "duration", duration);

        zval metadata_zv;
        ZVAL_COPY(&metadata_zv, &state->metadata);
        add_assoc_zval(&event, "metadata", &metadata_zv);

        zend_object *exception = EG(exception);
        if (exception != NULL) {
            zval exception_zv;
            ZVAL_OBJ_COPY(&exception_zv, exception);
            add_assoc_zval(&event, "exception", &exception_zv);
        } else {
            add_assoc_null(&event, "exception");
        }

        ZVAL_COPY_VALUE(&params[0], &event);

        ZVAL_COPY_VALUE(&params[1], &state->user_state);

        sentry_call_user_function_isolated(
            &SENTRY_G(end_callback),
            &retval,
            2,
            params
        );

        if (!Z_ISUNDEF(retval)) {
            zval_ptr_dtor(&retval);
        }

        zval_ptr_dtor(&event);
    }

    zend_hash_index_del(
        &SENTRY_G(active_calls),
        hash_key
    );
}

static zend_observer_fcall_handlers sentry_observer(zend_execute_data *execute_data) {
    zend_observer_fcall_handlers handlers = {0};

    if (sentry_should_observe(execute_data)) {
        handlers.begin = sentry_observer_begin;
        handlers.end = sentry_observer_end;
    }

    return handlers;
}

PHP_MINIT_FUNCTION(sentry) {
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(
        ce,
        "Sentry",
        "Trace",
        class_Sentry_Trace_methods
    );

    zend_class_entry *sentry_trace_attribute_ce = zend_register_internal_class(&ce);
    sentry_trace_attribute_ce->ce_flags |= ZEND_ACC_FINAL;

    zend_string *attribute_name = zend_string_init_interned(
        "Attribute",
        sizeof("Attribute")-1,
        1
    );

    zend_attribute *attribute = zend_add_class_attribute(
        sentry_trace_attribute_ce,
        attribute_name,
        1
    );

    zend_string_release(attribute_name);

    ZVAL_LONG(
        &attribute->args[0].value,
        ZEND_ATTRIBUTE_TARGET_FUNCTION | ZEND_ATTRIBUTE_TARGET_METHOD
    );

    sentry_trace_attribute_lcname = zend_string_init_interned(
        "sentry\\trace",
        sizeof("sentry\\trace") - 1,
        1
    );

    zend_observer_fcall_register(sentry_observer);

    return SUCCESS;
}

static PHP_GINIT_FUNCTION(sentry) {
#if defined(COMPILE_DL_SENTRY) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    memset(sentry_globals, 0, sizeof(*sentry_globals));
}

PHP_RINIT_FUNCTION(sentry) {
    SENTRY_G(in_callback) = false;
    zend_hash_init(&SENTRY_G(instrumented_functions), 8, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_init(&SENTRY_G(active_calls), 8, NULL, sentry_call_state_dtor, 0);

    ZVAL_UNDEF(&SENTRY_G(start_callback));
    ZVAL_UNDEF(&SENTRY_G(end_callback));

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(sentry) {
    zend_hash_destroy(&SENTRY_G(instrumented_functions));
    zend_hash_destroy(&SENTRY_G(active_calls));

    if (!Z_ISUNDEF(SENTRY_G(start_callback))) {
        zval_ptr_dtor(&SENTRY_G(start_callback));
        ZVAL_UNDEF(&SENTRY_G(start_callback));
    }

    if (!Z_ISUNDEF(SENTRY_G(end_callback))) {
        zval_ptr_dtor(&SENTRY_G(end_callback));
        ZVAL_UNDEF(&SENTRY_G(end_callback));
    }

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(sentry) {
    return SUCCESS;
}

zend_module_entry sentry_module_entry = {
    STANDARD_MODULE_HEADER,
    "sentry",
    ext_functions,
    PHP_MINIT(sentry),
    PHP_MSHUTDOWN(sentry),
    PHP_RINIT(sentry),
    PHP_RSHUTDOWN(sentry),
    NULL,
    "0.1.0",
    PHP_MODULE_GLOBALS(sentry),
    PHP_GINIT(sentry),
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_SENTRY
ZEND_GET_MODULE(sentry);
#endif
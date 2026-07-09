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
#include "sentry_log.h"

#define SENTRY_PREPROCESSING_ARG "preprocessing"
#define SENTRY_POSTPROCESSING_ARG "postprocessing"

void sentry_emit_log(int level, const char *message);

static void sentry_emit_logf(int level, const char *format, ...) ZEND_ATTRIBUTE_FORMAT(printf, 2, 3);

static void sentry_emit_callback_failure_log(
    const char *failure_log_message,
    zend_object *exception
);

static zend_string *sentry_build_display_name(zend_string *class_name, zend_string *function_name);

typedef struct {
    // Key Value store for metadata
    zval metadata;

    zval preprocessing_callback;

    zval postprocessing_callback;
} sentry_instrumented_function;

static void sentry_instrumented_function_free(sentry_instrumented_function *config) {
    zval_ptr_dtor(&config->metadata);

    if (!Z_ISUNDEF(config->preprocessing_callback)) {
        zval_ptr_dtor(&config->preprocessing_callback);
    }
    if (!Z_ISUNDEF(config->postprocessing_callback)) {
        zval_ptr_dtor(&config->postprocessing_callback);
    }
    efree(config);
}

static void sentry_instrumented_function_dtor(zval *zv) {
    sentry_instrumented_function_free(Z_PTR_P(zv));
}

/**
 * Resolved instrumentation for one zend_function. This struct is cached using
 * the function pointer so display names and metadata are not rebuilt on every
 * invocation.
 */
typedef struct {
    // display name for the declaring scope, never NULL
    zend_string *display_name;

    // metadata array from the registration or the attribute, never UNDEF
    zval metadata;

    // callback that is invoked with function parameters before the function runs
    // to produce metadata
    zval preprocessing_callback;

    // callback that is invoked with the return value after the function runs to
    // produce metadata
    zval postprocessing_callback;
} sentry_resolved_function;

static void sentry_resolved_function_free(sentry_resolved_function *resolved) {
    zend_string_release(resolved->display_name);
    zval_ptr_dtor(&resolved->metadata);

    zval_ptr_dtor(&resolved->preprocessing_callback);
    zval_ptr_dtor(&resolved->postprocessing_callback);
    efree(resolved);
}

static void sentry_resolved_function_dtor(zval *zv) {
    sentry_resolved_function_free(Z_PTR_P(zv));
}

ZEND_BEGIN_MODULE_GLOBALS(sentry)
    // Functions that should be observed. Values are sentry_instrumented_function pointers.
    HashTable instrumented_functions;

    // Resolved instrumentation cache, keyed by zend_function pointer. Values are
    // sentry_resolved_function pointers. Cleared whenever a new registration is added.
    HashTable resolved_functions;

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

    // User callback that is invoked when writing log messages.
    zval log_callback;

    // True when currently in a log callback. Used as reentry guard so that the log callback
    // cannot produce more logs and cause infinite invocations.
    bool in_log_callback;

    // True when the RSHUTDOWN ran and prevents access to potentially freed HashTables during shutdown
    bool shutting_down;
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

/**
 * zend_string includes the hash of the string so interning here
 * will save us calculating the hash for each function
 */
static zend_string *sentry_str_name;
static zend_string *sentry_str_start_time;
static zend_string *sentry_str_end_time;
static zend_string *sentry_str_duration;
static zend_string *sentry_str_metadata;
static zend_string *sentry_str_exception;

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
    zval postprocessing_callback;
} sentry_call_state;

static sentry_call_state *sentry_new_call_state(zend_string *name)
{
    struct timeval tv;
    (void) gettimeofday(&tv, NULL);

    sentry_call_state *state = emalloc(sizeof(sentry_call_state));
    state->name = name;
    state->start_time = tv.tv_sec + tv.tv_usec / 1000000.0;
    state->start_hrtime = zend_hrtime();
    ZVAL_UNDEF(&state->postprocessing_callback);

    return state;
}

/**
 * Destructor for the call state struct
 */
static void sentry_call_state_dtor(zval *zv) {
    sentry_call_state *state = Z_PTR_P(zv);

    zend_string_release(state->name);
    zval_ptr_dtor(&state->user_state);
    zval_ptr_dtor(&state->metadata);
    if (!Z_ISUNDEF(state->postprocessing_callback)) {
        zval_ptr_dtor(&state->postprocessing_callback);
    }

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
    if (EG(exception) != NULL || EG(prev_exception) != NULL) {
        zend_clear_exception();
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

        zend_string *arg_name = attribute->args[i].name;

        const zend_result result = zend_get_attribute_value(&attribute_arg, attribute, i, execute_data->func->common.scope);
        // result can be unsuccessful if e.g. constants are references that do not exist.
        if (result != SUCCESS) {
            zend_string *display_name = sentry_build_display_name(
                execute_data->func->common.scope == NULL ? NULL : execute_data->func->common.scope->name,
                execute_data->func->common.function_name
            );

            if (arg_name != NULL) {
                sentry_emit_logf(
                    SENTRY_LOG_WARNING,
                    "Sentry Trace attribute argument '%s' on '%s' could not be evaluated and was ignored.",
                    ZSTR_VAL(arg_name),
                    ZSTR_VAL(display_name)
                );
            } else {
                sentry_emit_logf(
                    SENTRY_LOG_WARNING,
                    "Sentry Trace attribute argument #%u on '%s' could not be evaluated and was ignored.",
                    i + 1,
                    ZSTR_VAL(display_name)
                );
            }

            zend_string_release(display_name);

            sentry_clear_pending_exception();
            continue;
        }

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
    zval *params,
    const char *failure_log_message
) {
    // call_user_function may leave retval untouched on failure
    ZVAL_UNDEF(retval);

    bool was_in_callback = SENTRY_G(in_callback);
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
        if (failure_log_message != NULL) {
            sentry_emit_callback_failure_log(failure_log_message, suppressed);
        }
        OBJ_RELEASE(suppressed);
    }

    SENTRY_G(in_callback) = was_in_callback;
}

// ===== EXCEPTION ISOLATION END ====

static zend_string *sentry_join_class_function(
    const zend_string *class_name,
    const zend_string *function_name,
    const bool lowercase
) {
    size_t class_len = ZSTR_LEN(class_name);
    size_t function_len = ZSTR_LEN(function_name);
    zend_string *result = zend_string_alloc(class_len + 2 + function_len, 0);
    char *dest = ZSTR_VAL(result);

    if (lowercase) {
        zend_str_tolower_copy(dest, ZSTR_VAL(class_name), class_len);
        zend_str_tolower_copy(dest + class_len + 2, ZSTR_VAL(function_name), function_len);
    } else {
        memcpy(dest, ZSTR_VAL(class_name), class_len);
        memcpy(dest + class_len + 2, ZSTR_VAL(function_name), function_len);
        dest[class_len + 2 + function_len] = '\0';
    }

    dest[class_len] = ':';
    dest[class_len + 1] = ':';

    return result;
}

static zend_string *sentry_build_display_name(zend_string *class_name, zend_string *function_name) {
    if (function_name == NULL) {
        return NULL;
    }

    if (class_name == NULL) {
        return zend_string_copy(function_name);
    }
    return sentry_join_class_function(class_name, function_name, /* lowercase */ false);
}

static zend_string *sentry_build_key(zend_string *class_name, zend_string *function_name) {
    if (function_name == NULL) {
        return NULL;
    }

    if (class_name == NULL) {
        return zend_string_tolower(function_name);
    }

    return sentry_join_class_function(class_name, function_name, /* lowercase */ true);
}

ZEND_FUNCTION(Sentry_instrument) {
    zend_string *class_name = NULL;
    zend_string *function_name;

    zval *metadata_args = NULL;
    uint32_t metadata_argc = 0;
    HashTable *named_metadata = NULL;

    ZEND_PARSE_PARAMETERS_START(2,-1)
        Z_PARAM_STR_OR_NULL(class_name)
        Z_PARAM_STR(function_name)
        Z_PARAM_VARIADIC_WITH_NAMED(metadata_args, metadata_argc, named_metadata)
    ZEND_PARSE_PARAMETERS_END();

    if (SENTRY_G(shutting_down) || SENTRY_G(in_callback)) {
        RETURN_FALSE;
    }

    zend_function *target_func = NULL;

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
            if (func != NULL) {
                target_func = func;
                if (func->common.scope != NULL) {
                    class_name = func->common.scope->name;
                }
            }
            zend_string_release(lc_func);
        }
    }

    zval metadata;
    array_init(&metadata);

    zval preprocessing_callback;
    ZVAL_UNDEF(&preprocessing_callback);

    zval postprocessing_callback;
    ZVAL_UNDEF(&postprocessing_callback);

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
            zval *callback_target = NULL;

            if (name != NULL) {
                if (zend_string_equals_literal(name, SENTRY_PREPROCESSING_ARG)) {
                    callback_target = &preprocessing_callback;
                } else if (zend_string_equals_literal(name, SENTRY_POSTPROCESSING_ARG)) {
                    callback_target = &postprocessing_callback;
                }
            }

            if (callback_target != NULL) {
                if (!zend_is_callable(value, 0, NULL)) {
                    zend_string *display_name = sentry_build_display_name(class_name, function_name);
                    sentry_emit_logf(
                        SENTRY_LOG_WARNING,
                        "Sentry instrumentation argument \"%s\" for '%s' is not a valid callback and was ignored.",
                        ZSTR_VAL(name),
                        ZSTR_VAL(display_name)
                    );
                    zend_string_release(display_name);
                    continue;
                }
                ZVAL_COPY(callback_target, value);
                continue;
            }

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

    if (class_name == NULL) {
        target_func = zend_hash_find_ptr(EG(function_table), key);
    }

    sentry_instrumented_function *config = emalloc(sizeof(sentry_instrumented_function));
    config->metadata = metadata;
    config->preprocessing_callback = preprocessing_callback;
    config->postprocessing_callback = postprocessing_callback;

    zval config_zv;
    ZVAL_PTR(&config_zv, config);

    const zval* inserted = zend_hash_add(&SENTRY_G(instrumented_functions), key, &config_zv);

    // If the element wasn't inserted we have to manually destroy the local value to prevent memory leaks
    if (inserted == NULL) {
        zend_string *display_name = sentry_build_display_name(class_name, function_name);
        sentry_emit_logf(
            SENTRY_LOG_DEBUG,
            "Sentry instrumentation target '%s' is already registered and was ignored.",
            ZSTR_VAL(display_name)
        );
        zend_string_release(display_name);

        sentry_instrumented_function_free(config);
    } else if (target_func != NULL) {
        zend_hash_index_del(
            &SENTRY_G(resolved_functions),
            (zend_ulong) (uintptr_t) target_func
        );
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

    if (SENTRY_G(shutting_down)) {
        RETURN_FALSE;
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

    if (SENTRY_G(shutting_down)) {
        RETURN_FALSE;
    }

    if (!Z_ISUNDEF(SENTRY_G(start_callback))) {
        zval_ptr_dtor(&SENTRY_G(start_callback));
    }

    ZVAL_COPY(&SENTRY_G(start_callback), callback);

    RETURN_TRUE;
}

ZEND_FUNCTION(Sentry_setLogCallback) {
    zval *callback;

    ZEND_PARSE_PARAMETERS_START(1,1)
        Z_PARAM_ZVAL(callback)
    ZEND_PARSE_PARAMETERS_END();

    if (!zend_is_callable(callback, 0, NULL)) {
        zend_argument_type_error(1, "must be a valid callback");
        RETURN_THROWS();
    }

    if (SENTRY_G(shutting_down)) {
        RETURN_FALSE;
    }

    if (!Z_ISUNDEF(SENTRY_G(log_callback))) {
        zval_ptr_dtor(&SENTRY_G(log_callback));
    }

    ZVAL_COPY(&SENTRY_G(log_callback), callback);

    RETURN_TRUE;
}

void sentry_emit_log(int level, const char *message) {
    if (Z_ISUNDEF(SENTRY_G(log_callback))) {
        return;
    }
    if (SENTRY_G(in_log_callback)) {
        return;
    }
    SENTRY_G(in_log_callback) = true;

    zval retval;

    zval params[2];
    ZVAL_LONG(&params[0], level);
    ZVAL_STRING(&params[1], message);

    sentry_call_user_function_isolated(
        &SENTRY_G(log_callback),
        &retval,
        2,
        params,
        NULL
    );

    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&params[1]);
    SENTRY_G(in_log_callback) = false;
}

static void sentry_emit_logf(int level, const char *format, ...) {
    if (Z_ISUNDEF(SENTRY_G(log_callback)) || SENTRY_G(in_log_callback)) {
        return;
    }

    va_list args;
    va_start(args, format);
    zend_string *message = zend_vstrpprintf(0, format, args);
    va_end(args);

    sentry_emit_log(level, ZSTR_VAL(message));
    zend_string_release(message);
}

static void sentry_emit_callback_failure_log(
    const char *failure_log_message,
    zend_object *exception
) {
    zend_string *message = sentry_format_exception_log_message(failure_log_message, exception);

    sentry_emit_log(SENTRY_LOG_ERROR, ZSTR_VAL(message));
    zend_string_release(message);
}

static sentry_instrumented_function *sentry_find_registration(const zend_function *func) {
    if (zend_hash_num_elements(&SENTRY_G(instrumented_functions)) == 0) {
        return NULL;
    }

    zend_string *key = sentry_build_key(
        func->common.scope == NULL ? NULL : func->common.scope->name,
        func->common.function_name
    );
    zval *config_zv = zend_hash_find(&SENTRY_G(instrumented_functions), key);
    zend_string_release(key);

    return config_zv == NULL ? NULL : Z_PTR_P(config_zv);
}

static sentry_resolved_function *sentry_resolve_function(zend_execute_data *execute_data) {
    const zend_function *func = execute_data->func;

    if (func->common.function_name == NULL) {
        return NULL;
    }

    sentry_instrumented_function *config = sentry_find_registration(func);

    zval metadata;
    if (config != NULL) {
        ZVAL_COPY(&metadata, &config->metadata);
    } else {
        if (!sentry_has_trace_attribute(execute_data)) {
            return NULL;
        }
        sentry_get_attribute_metadata(execute_data, &metadata);
    }

    sentry_resolved_function *resolved = emalloc(sizeof(sentry_resolved_function));
    resolved->display_name = sentry_build_display_name(
        func->common.scope == NULL ? NULL : func->common.scope->name,
        func->common.function_name
    );
    resolved->metadata = metadata;
    if (config != NULL) {
        ZVAL_COPY(&resolved->preprocessing_callback, &config->preprocessing_callback);
        ZVAL_COPY(&resolved->postprocessing_callback, &config->postprocessing_callback);
    } else {
        ZVAL_UNDEF(&resolved->preprocessing_callback);
        ZVAL_UNDEF(&resolved->postprocessing_callback);
    }

    zval resolved_zv;
    ZVAL_PTR(&resolved_zv, resolved);

    zend_hash_index_update(
        &SENTRY_G(resolved_functions),
        (zend_ulong) (uintptr_t) func,
        &resolved_zv
    );

    return resolved;
}

static zval *sentry_get_call_argument(zend_execute_data *execute_data, uint32_t index) {
    const zend_function *func = execute_data->func;

    if (ZEND_USER_CODE(func->type) && index >= func->op_array.num_args) {
        zval *extra_args = ZEND_CALL_VAR_NUM(
            execute_data,
            func->op_array.last_var + func->op_array.T
        );
        return extra_args + (index - func->op_array.num_args);
    }

    return ZEND_CALL_ARG(execute_data, index + 1);
}

/**
 * Merges all data from retval and stores them in metadata if retval is an array.
 * If not, it will emit a warning message.
 */
static void sentry_apply_processing_result(
    zval *retval,
    zval *metadata,
    const char *warning_message
) {
    if (Z_TYPE_P(retval) == IS_ARRAY) {
        SEPARATE_ARRAY(metadata);
        sentry_merge_array(metadata, retval);
    } else if (!Z_ISUNDEF_P(retval) && Z_TYPE_P(retval) != IS_NULL) {
        sentry_emit_log(SENTRY_LOG_WARNING, warning_message);
    }
}

/**
 * Collects all parameters from the currently instrumented function and invokes
 * the passed callback with those parameters.
 * Merged any array shaped data into metadata so it's available in the start and end callback.
 */
static void sentry_run_preprocessing_callback(
    zend_execute_data *execute_data,
    zval *callback,
    zval *metadata
) {
    uint32_t argument_count = ZEND_CALL_NUM_ARGS(execute_data);
    zval *params = safe_emalloc(argument_count, sizeof(zval), 0);

    for (uint32_t i = 0; i < argument_count; i++) {
        zval *argument = sentry_get_call_argument(execute_data, i);

        if (Z_TYPE_P(argument) == IS_UNDEF) {
            ZVAL_NULL(&params[i]);
        } else {
            ZVAL_COPY(&params[i], argument);
        }
    }

    zval retval;

    sentry_call_user_function_isolated(
        callback,
        &retval,
        argument_count,
        params,
        "Sentry preprocessing callback threw an exception and was ignored."
    );

    for (uint32_t i = 0; i < argument_count; i++) {
        zval_ptr_dtor(&params[i]);
    }
    efree(params);

    sentry_apply_processing_result(
        &retval,
        metadata,
        "Sentry preprocessing callback returned a non-array value and was ignored."
    );

    zval_ptr_dtor(&retval);
}

/**
 * Captures the return value of the instrumented function if no exception was thrown
 * and invokes the passed callback with it.
 * Merged any array shaped data into metadata so it's available in the end callback.
 */
static void sentry_run_postprocessing_callback(
    zval *return_value,
    sentry_call_state *state
) {
    if (Z_ISUNDEF(state->postprocessing_callback) || EG(exception) != NULL) {
        return;
    }

    // if return_value is NULL or undefined, it means that the function failed to
    // return at all. One scenarios when this happens is when an exception is thrown
    if (return_value == NULL || Z_ISUNDEF_P(return_value)) {
        return;
    }

    zval params[1];
    ZVAL_COPY(&params[0], return_value);

    zval retval;

    sentry_call_user_function_isolated(
        &state->postprocessing_callback,
        &retval,
        1,
        params,
        "Sentry postprocessing callback threw an exception and was ignored."
    );

    zval_ptr_dtor(&params[0]);

    sentry_apply_processing_result(
        &retval,
        &state->metadata,
        "Sentry postprocessing callback returned a non-array value and was ignored."
    );
    zval_ptr_dtor(&retval);
}

static void sentry_observer_begin(zend_execute_data *execute_data) {
    if (SENTRY_G(in_callback) || SENTRY_G(shutting_down)) {
        return;
    }

    const zend_function *func = execute_data->func;
    zval *resolved_zv = zend_hash_index_find(
        &SENTRY_G(resolved_functions),
        (zend_ulong) (uintptr_t) func
    );
    sentry_resolved_function *resolved = resolved_zv == NULL ? NULL : Z_PTR_P(resolved_zv);

    if (resolved == NULL) {
        resolved = sentry_resolve_function(execute_data);
        if (resolved == NULL) {
            return;
        }
    }

    // the display name has to be rebuilt here because a subclass can call
    // an instrumented parent function, in which case both point to the same
    // function and it would produce the name of the parent
    zend_string *name;
    zend_class_entry *called_scope = zend_get_called_scope(execute_data);
    if (called_scope == func->common.scope) {
        name = zend_string_copy(resolved->display_name);
    } else {
        name = sentry_build_display_name(
            called_scope == NULL ? NULL : called_scope->name,
            func->common.function_name
        );
    }

    sentry_call_state *state = sentry_new_call_state(name);
    ZVAL_COPY(&state->metadata, &resolved->metadata);
    ZVAL_COPY(&state->postprocessing_callback, &resolved->postprocessing_callback);

    zval preprocessing_callback;
    ZVAL_COPY(&preprocessing_callback, &resolved->preprocessing_callback);
    resolved = NULL;

    if (!Z_ISUNDEF(preprocessing_callback)) {
        sentry_run_preprocessing_callback(
            execute_data,
            &preprocessing_callback,
            &state->metadata
        );
        zval_ptr_dtor(&preprocessing_callback);
    }

    zval retval;
    ZVAL_UNDEF(&retval);

    if (!Z_ISUNDEF(SENTRY_G(start_callback))) {
        zval data;
        array_init(&data);

        zval tmp;
        ZVAL_STR_COPY(&tmp, name);
        zend_hash_add_new(Z_ARRVAL(data), sentry_str_name, &tmp);
        ZVAL_DOUBLE(&tmp, state->start_time);
        zend_hash_add_new(Z_ARRVAL(data), sentry_str_start_time, &tmp);
        ZVAL_COPY(&tmp, &state->metadata);
        zend_hash_add_new(Z_ARRVAL(data), sentry_str_metadata, &tmp);

        zval params[1];
        ZVAL_COPY_VALUE(&params[0], &data);

        sentry_call_user_function_isolated(
            &SENTRY_G(start_callback),
            &retval,
            1,
            params,
            "Sentry start callback threw an exception and was ignored."
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
}

static void sentry_observer_end(zend_execute_data *execute_data, zval *return_value) {
    if (SENTRY_G(shutting_down)) {
        return;
    }

    zend_ulong hash_key = (zend_ulong) (uintptr_t) execute_data;

    zval *state_zv = zend_hash_index_find(&SENTRY_G(active_calls), hash_key);

    if (state_zv == NULL) {
        return;
    }

    sentry_call_state *state = Z_PTR_P(state_zv);

    zend_hrtime_t elapsed_ns = zend_hrtime() - state->start_hrtime;
    double duration = elapsed_ns / 1000000.0;
    double end_time = state->start_time + (duration / 1000.0);

    sentry_run_postprocessing_callback(return_value, state);

    if (!Z_ISUNDEF(SENTRY_G(end_callback))) {
        zval event;
        zval retval;

        // 1. array with instrumented data + metadata
        // 2. whatever was returned in the start callback
        zval params[2];

        array_init(&event);

        zval tmp;
        ZVAL_STR_COPY(&tmp, state->name);
        zend_hash_add_new(Z_ARRVAL(event), sentry_str_name, &tmp);
        ZVAL_DOUBLE(&tmp, state->start_time);
        zend_hash_add_new(Z_ARRVAL(event), sentry_str_start_time, &tmp);
        ZVAL_DOUBLE(&tmp, end_time);
        zend_hash_add_new(Z_ARRVAL(event), sentry_str_end_time, &tmp);
        ZVAL_DOUBLE(&tmp, duration);
        zend_hash_add_new(Z_ARRVAL(event), sentry_str_duration, &tmp);
        ZVAL_COPY(&tmp, &state->metadata);
        zend_hash_add_new(Z_ARRVAL(event), sentry_str_metadata, &tmp);

        zend_object *exception = EG(exception);
        if (exception != NULL) {
            ZVAL_OBJ_COPY(&tmp, exception);
        } else {
            ZVAL_NULL(&tmp);
        }
        zend_hash_add_new(Z_ARRVAL(event), sentry_str_exception, &tmp);

        ZVAL_COPY_VALUE(&params[0], &event);

        ZVAL_COPY_VALUE(&params[1], &state->user_state);

        sentry_call_user_function_isolated(
            &SENTRY_G(end_callback),
            &retval,
            2,
            params,
            "Sentry end callback threw an exception and was ignored."
        );

        zval_ptr_dtor(&retval);
        zval_ptr_dtor(&event);
    }

    zend_hash_index_del(
        &SENTRY_G(active_calls),
        hash_key
    );
}

static zend_observer_fcall_handlers sentry_observer(zend_execute_data *execute_data) {
    zend_observer_fcall_handlers handlers = {0};

    if (SENTRY_G(shutting_down)) {
        return handlers;
    }

    const zend_function *func = execute_data->func;

    // prevent closures from being instrumented. technically possible
    // with the attribute but the architecture is not build
    // around ephemeral function pointer
    if (func->common.function_name == NULL
        || (func->common.fn_flags & ZEND_ACC_CLOSURE)
        || (func->common.fn_flags & ZEND_ACC_CALL_VIA_TRAMPOLINE)) {
        return handlers;
    }

    if (sentry_resolve_function(execute_data) != NULL) {
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

    sentry_str_name = zend_string_init_interned("name", sizeof("name") - 1, 1);
    sentry_str_start_time = zend_string_init_interned("start_time", sizeof("start_time") - 1, 1);
    sentry_str_end_time = zend_string_init_interned("end_time", sizeof("end_time") - 1, 1);
    sentry_str_duration = zend_string_init_interned("duration", sizeof("duration") - 1, 1);
    sentry_str_metadata = zend_string_init_interned("metadata", sizeof("metadata") - 1, 1);
    sentry_str_exception = zend_string_init_interned("exception", sizeof("exception") - 1, 1);

    sentry_register_log_constants(module_number);

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
    SENTRY_G(in_log_callback) = false;
    SENTRY_G(shutting_down) = false;
    zend_hash_init(&SENTRY_G(instrumented_functions), 8, NULL, sentry_instrumented_function_dtor, 0);
    zend_hash_init(&SENTRY_G(resolved_functions), 8, NULL, sentry_resolved_function_dtor, 0);
    zend_hash_init(&SENTRY_G(active_calls), 8, NULL, sentry_call_state_dtor, 0);

    ZVAL_UNDEF(&SENTRY_G(start_callback));
    ZVAL_UNDEF(&SENTRY_G(end_callback));
    ZVAL_UNDEF(&SENTRY_G(log_callback));

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(sentry) {
    SENTRY_G(shutting_down) = true;

    zend_hash_destroy(&SENTRY_G(resolved_functions));
    zend_hash_destroy(&SENTRY_G(instrumented_functions));
    zend_hash_destroy(&SENTRY_G(active_calls));

    zval_ptr_dtor(&SENTRY_G(start_callback));
    ZVAL_UNDEF(&SENTRY_G(start_callback));

    zval_ptr_dtor(&SENTRY_G(end_callback));
    ZVAL_UNDEF(&SENTRY_G(end_callback));

    zval_ptr_dtor(&SENTRY_G(log_callback));
    ZVAL_UNDEF(&SENTRY_G(log_callback));

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

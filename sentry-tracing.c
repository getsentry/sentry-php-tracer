#include "php.h"
#include "Zend/zend_observer.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_attributes.h"
#include <stdint.h>

/**
 * Stores the functions that should be observed. If a function is not in this map,
 * it will not be measured.
 */
static HashTable sentry_tracing_hook_keys;

/**
 * Stores the call state of currently active functions. Mainly used to store start time
 * to be able to calculate duration after invocation ends.
 */
static HashTable sentry_tracing_active_calls;

/**
 * This callback is invoked before a function is observed. The return value is stored and
 * will be passed into the end callback, so specific data can be stored between the callbacks.
 */
static zval sentry_tracing_start_callback;

/**
 * This callback is invoked whenever the observer finishes. It will be invoked with
 * the call state so that the PHP part can create new Spans etc.
 */
static zval sentry_tracing_end_callback;

static zend_class_entry *sentry_instrumented_attribute_ce;

/**
 * Holds the name of the \Sentry\Instrumented attribute that can be used to retrieve it
 * using zend_get_attribute.
 */
static zend_string *sentry_instrumented_attribute_lcname;

// ==== CALL STATE BEGIN ====

/**
 * Tracks the state per function/method call. We need this to calculate how long an invocation took
 * and to produce proper spans
 */
typedef struct {
    zend_string *name;
    double start_time;
    zval user_state;
    zval metadata;
} sentry_tracing_call_state;

/**
 * Destructor for the call state struct
 */
static void sentry_tracing_call_state_dtor(zval *zv) {
    sentry_tracing_call_state *state = Z_PTR_P(zv);

    if (state->name != NULL) {
        zend_string_release(state->name);
    }

    zval_ptr_dtor(&state->user_state);
    zval_ptr_dtor(&state->metadata);

    efree(state);
}

// ==== CALL STATE END ====

// ==== ATTRIBUTE START ====
static zend_attribute *sentry_tracing_get_instrumented_attribute(zend_execute_data *execute_data) {
    const zend_function *func = execute_data->func;

    if (func->common.attributes == NULL) {
        return NULL;
    }

    zend_attribute *attribute = zend_get_attribute(
        func->common.attributes,
        sentry_instrumented_attribute_lcname
    );

    return attribute;
}

static bool sentry_tracing_has_instrumented_attribute(zend_execute_data *execute_data) {
    return sentry_tracing_get_instrumented_attribute(execute_data) != NULL;
}

static void sentry_tracing_get_attribute_metadata(
    zend_execute_data *execute_data,
    zval *metadata
) {
    array_init(metadata);

    zend_attribute *attribute = sentry_tracing_get_instrumented_attribute(execute_data);
    if (attribute != NULL && attribute->argc > 0) {
        zval attribute_arg;
        ZVAL_UNDEF(&attribute_arg);

        if (zend_get_attribute_value(
            &attribute_arg,
            attribute,
            0,
            execute_data->func->common.scope
        ) == SUCCESS && Z_TYPE(attribute_arg) == IS_ARRAY) {
            zend_hash_copy(
                Z_ARRVAL_P(metadata),
                Z_ARRVAL(attribute_arg),
                zval_add_ref
            );
        }

        if (!Z_ISUNDEF(attribute_arg)) {
            zval_ptr_dtor(&attribute_arg);
        }
    }
}


PHP_METHOD(Sentry_Instrumented, __construct) {
    zval *metadata = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(metadata)
    ZEND_PARSE_PARAMETERS_END();
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_sentry_instrumented_attribute_construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, metadata, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO();

static const zend_function_entry sentry_instrumented_attribute_methods[] = {
    ZEND_ME(Sentry_Instrumented, __construct, arginfo_sentry_instrumented_attribute_construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

// ==== ATTRIBUTE END ======

/**
 * Returns the current timestamp as float. Equivalent to microtime(true) in PHP.
 */
static double current_timestamp_as_float() {
    return zend_hrtime() / 1000000000.0;
}

static zend_result call_user_function_ignore(zval *callback, zval *retval, uint32_t param_count, zval *params) {
    zend_result result = call_user_function(
        EG(function_table),
        NULL,
        callback,
        retval,
        param_count,
        params
    );

    if (EG(exception) != NULL) {
        zend_clear_exception();
    }
    return result;
}

static zend_string *sentry_tracing_build_display_name(
    zend_execute_data *execute_data
) {
    const zend_function *func = execute_data->func;

    if (func->common.function_name == NULL) {
        return NULL;
    }

    if (func->common.scope != NULL) {
        return zend_strpprintf(
            0,
            "%s::%s",
            ZSTR_VAL(func->common.scope->name),
            ZSTR_VAL(func->common.function_name)
        );
    }

    return zend_string_copy(func->common.function_name);
}

PHP_FUNCTION(sentry_tracing_hook) {
    zend_string *class_name = NULL;
    zend_string *function_name;
    zend_string *key;
    zval metadata;
    zval *extra_metadata = NULL;

    ZEND_PARSE_PARAMETERS_START(2,3)
        Z_PARAM_STR_OR_NULL(class_name)
        Z_PARAM_STR(function_name)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(extra_metadata)
    ZEND_PARSE_PARAMETERS_END();

    array_init(&metadata);

    if (extra_metadata != NULL) {
        zend_hash_copy(
            Z_ARRVAL(metadata),
            Z_ARRVAL_P(extra_metadata),
            zval_add_ref
        );
    }

    if (class_name != NULL) {
        key = zend_strpprintf(
            0,
            "%s::%s",
            ZSTR_VAL(class_name),
            ZSTR_VAL(function_name)
        );
    } else {
        key = zend_string_copy(function_name);
    }

    zend_string *lowercase_key = zend_string_tolower(key);
    zend_string_release(key);

    const zval* inserted = zend_hash_add(&sentry_tracing_hook_keys, lowercase_key, &metadata);

    // If the element wasn't inserted we have to manually destroy the local value to prevent memory leaks
    if (inserted == NULL) {
        zval_ptr_dtor(&metadata);
    }

    zend_string_release(lowercase_key);

    RETURN_BOOL(inserted != NULL);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_sentry_tracing_hook_key,
    0,
    2,
    _IS_BOOL,
    0
)
    ZEND_ARG_TYPE_INFO(0, class_name, IS_STRING, 1)
    ZEND_ARG_TYPE_INFO(0, function_name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, extra_metadata, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO();




PHP_FUNCTION(sentry_tracing_set_end_callback) {
    zval *callback;

    ZEND_PARSE_PARAMETERS_START(1,1)
        Z_PARAM_ZVAL(callback)
    ZEND_PARSE_PARAMETERS_END();

    if (!zend_is_callable(callback, 0, NULL)) {
        zend_argument_type_error(1, "must be a valid callback");
        RETURN_THROWS();
    }

    if (!Z_ISUNDEF(sentry_tracing_end_callback)) {
        zval_ptr_dtor(&sentry_tracing_end_callback);
    }

    ZVAL_COPY(&sentry_tracing_end_callback, callback);

    RETURN_TRUE;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_sentry_tracing_set_end_callback,
    0,
    1,
    _IS_BOOL,
    0
)
    ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
ZEND_END_ARG_INFO();


PHP_FUNCTION(sentry_tracing_set_start_callback) {
    zval *callback;

    ZEND_PARSE_PARAMETERS_START(1,1)
        Z_PARAM_ZVAL(callback)
    ZEND_PARSE_PARAMETERS_END();

    if (!zend_is_callable(callback, 0, NULL)) {
        zend_argument_type_error(1, "must be a valid callback");
        RETURN_THROWS();
    }

    if (!Z_ISUNDEF(sentry_tracing_start_callback)) {
        zval_ptr_dtor(&sentry_tracing_start_callback);
    }

    ZVAL_COPY(&sentry_tracing_start_callback, callback);

    RETURN_TRUE;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_sentry_tracing_set_start_callback,
    0,
    1,
    _IS_BOOL,
    0
)
    ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
ZEND_END_ARG_INFO();

/**
 * Returns a normalized function key that can be used as unique identifier to register hooks for it.
 * The key will always be in lowercase to support the case insensitivity of PHP functions.
 */
static zend_string *sentry_tracing_build_function_key(zend_execute_data *execute_data) {
    const zend_function *func = execute_data->func;
    zend_string *key;

    if (func->common.function_name == NULL) {
        return NULL;
    }

    if (func->common.scope != NULL) {
        key = strpprintf(
            0,
            "%s::%s",
            ZSTR_VAL(func->common.scope->name),
            ZSTR_VAL(func->common.function_name)
        );
    } else {
        key = zend_string_copy(func->common.function_name);
    }

    zend_string *lowercase_key = zend_string_tolower(key);

    zend_string_release(key);

    return lowercase_key;
}

static bool sentry_tracing_should_observe(zend_execute_data *execute_data) {
    zend_string *key = sentry_tracing_build_function_key(execute_data);

    if (key == NULL) {
        return false;
    }

    const bool explicitly_hooked = zend_hash_exists(&sentry_tracing_hook_keys, key);

    zend_string_release(key);

    if (explicitly_hooked) {
        return true;
    }

    return sentry_tracing_has_instrumented_attribute(execute_data);
}

static void sentry_tracing_observer_begin(zend_execute_data *execute_data) {
    zend_string *key = sentry_tracing_build_function_key(execute_data);
    if (key == NULL) {
        return;
    }

    zval default_metadata;
    bool using_default_metadata = false;

    zval *metadata = zend_hash_find(&sentry_tracing_hook_keys, key);

    if (metadata == NULL) {
        sentry_tracing_get_attribute_metadata(execute_data, &default_metadata);
        metadata = &default_metadata;
        using_default_metadata = true;
    }

    if (Z_TYPE_P(metadata) == IS_ARRAY) {
        zend_string *name = sentry_tracing_build_display_name(execute_data);
        if (name == NULL) {
            zend_string_release(key);
            return;
        }

        zval state_zv;
        zval retval;
        ZVAL_UNDEF(&retval);

        if (!Z_ISUNDEF(sentry_tracing_start_callback)) {
            zval params[1];

            ZVAL_COPY(&params[0], metadata);

            zend_result success = call_user_function_ignore(
                &sentry_tracing_start_callback,
                &retval,
                1,
                params
            );

            zval_ptr_dtor(&params[0]);

            if (success != SUCCESS || Z_ISUNDEF(retval)) {
                ZVAL_NULL(&retval);
            }
        }

        sentry_tracing_call_state *state = emalloc(sizeof(sentry_tracing_call_state));
        state->name = name;
        state->start_time = current_timestamp_as_float();

        ZVAL_COPY(&state->metadata, metadata);

        if (Z_ISUNDEF(retval)) {
            ZVAL_NULL(&retval);
        }
        state->user_state = retval;


        ZVAL_PTR(&state_zv, state);


        zend_hash_index_update(
            &sentry_tracing_active_calls,
            (zend_ulong) (uintptr_t) execute_data,
            &state_zv
        );
    }

    if (using_default_metadata) {
        zval_ptr_dtor(&default_metadata);
    }

    zend_string_release(key);
}


static void sentry_tracing_observer_end(zend_execute_data *execute_data, zval *return_value) {
    zend_ulong hash_key = (zend_ulong) (uintptr_t) execute_data;

    if (EG(exception) != NULL) {
        zend_hash_index_del(
            &sentry_tracing_active_calls,
            hash_key
        );
        return;
    }

    zval *state_zv = zend_hash_index_find(&sentry_tracing_active_calls, hash_key);

    if (state_zv == NULL || Z_TYPE_P(state_zv) != IS_PTR) {
        return;
    }

    sentry_tracing_call_state *state = Z_PTR_P(state_zv);

    double end_time = current_timestamp_as_float();
    double duration = end_time - state->start_time;

    if (!Z_ISUNDEF(sentry_tracing_end_callback)) {
        zval event;
        zval retval;
        ZVAL_UNDEF(&retval);
        zval params[2];

        array_init(&event);

        if (state->name != NULL) {
            add_assoc_str(&event, "name", zend_string_copy(state->name));
        }

        add_assoc_double(&event, "start_time", state->start_time);
        add_assoc_double(&event, "end_time", end_time);
        add_assoc_double(&event, "duration", duration);

        zval metadata_zv;
        ZVAL_COPY(&metadata_zv, &state->metadata);
        add_assoc_zval(&event, "metadata", &metadata_zv);

        ZVAL_COPY_VALUE(&params[0], &event);

        ZVAL_COPY(&params[1], &state->user_state);

        call_user_function_ignore(
            &sentry_tracing_end_callback,
            &retval,
            2,
            params
        );

        if (!Z_ISUNDEF(retval)) {
            zval_ptr_dtor(&retval);
        }

        zval_ptr_dtor(&params[1]);
        zval_ptr_dtor(&event);
    }

    zend_hash_index_del(
        &sentry_tracing_active_calls,
        hash_key
    );
}

static zend_observer_fcall_handlers sentry_tracing_observer(zend_execute_data *execute_data) {
    zend_observer_fcall_handlers handlers = {0};

    if (sentry_tracing_should_observe(execute_data)) {
        handlers.begin = sentry_tracing_observer_begin;
        handlers.end = sentry_tracing_observer_end;
    }

    return handlers;
}

PHP_MINIT_FUNCTION(sentry_tracing) {
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(
        ce,
        "Sentry",
        "Instrumented",
        sentry_instrumented_attribute_methods
    );

    sentry_instrumented_attribute_ce = zend_register_internal_class(&ce);
    sentry_instrumented_attribute_ce->ce_flags |= ZEND_ACC_FINAL;

    zend_string *attribute_name = zend_string_init_interned(
        "Attribute",
        sizeof("Attribute")-1,
        1
    );

    zend_attribute *attribute = zend_add_class_attribute(
        sentry_instrumented_attribute_ce,
        attribute_name,
        1
    );

    zend_string_release(attribute_name);

    ZVAL_LONG(
        &attribute->args[0].value,
        ZEND_ATTRIBUTE_TARGET_FUNCTION | ZEND_ATTRIBUTE_TARGET_METHOD
    );

    sentry_instrumented_attribute_lcname = zend_string_init_interned(
        "sentry\\instrumented",
        sizeof("sentry\\instrumented") - 1,
        1
    );

    zend_observer_fcall_register(sentry_tracing_observer);

    return SUCCESS;
}

PHP_RINIT_FUNCTION(sentry_tracing) {
    zend_hash_init(&sentry_tracing_hook_keys, 8, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_init(&sentry_tracing_active_calls, 8, NULL, sentry_tracing_call_state_dtor, 0);

    ZVAL_UNDEF(&sentry_tracing_start_callback);
    ZVAL_UNDEF(&sentry_tracing_end_callback);

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(sentry_tracing) {
    zend_hash_destroy(&sentry_tracing_hook_keys);
    zend_hash_destroy(&sentry_tracing_active_calls);

    if (!Z_ISUNDEF(sentry_tracing_start_callback)) {
        zval_ptr_dtor(&sentry_tracing_start_callback);
        ZVAL_UNDEF(&sentry_tracing_start_callback);
    }

    if (!Z_ISUNDEF(sentry_tracing_end_callback)) {
        zval_ptr_dtor(&sentry_tracing_end_callback);
        ZVAL_UNDEF(&sentry_tracing_end_callback);
    }

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(sentry_tracing) {
    if (sentry_instrumented_attribute_lcname != NULL) {
        zend_string_release(sentry_instrumented_attribute_lcname);
        sentry_instrumented_attribute_lcname = NULL;
    }

    return SUCCESS;
}


static const zend_function_entry sentry_tracing_functions[] = {
    ZEND_NS_FENTRY("Sentry\\Instrumentation", hook, ZEND_FN(sentry_tracing_hook), arginfo_sentry_tracing_hook_key, 0)
    ZEND_NS_FENTRY("Sentry\\Instrumentation", setEndCallback,  ZEND_FN(sentry_tracing_set_end_callback), arginfo_sentry_tracing_set_end_callback, 0)
    ZEND_NS_FENTRY("Sentry\\Instrumentation", setStartCallback, ZEND_FN(sentry_tracing_set_start_callback), arginfo_sentry_tracing_set_start_callback, 0)
    PHP_FE_END
};

zend_module_entry sentry_module_entry = {
    STANDARD_MODULE_HEADER,
    "sentry",
    sentry_tracing_functions,
    PHP_MINIT(sentry_tracing),
    PHP_MSHUTDOWN(sentry_tracing),
    PHP_RINIT(sentry_tracing),
    PHP_RSHUTDOWN(sentry_tracing),
    NULL,
    "0.1.0",
    STANDARD_MODULE_PROPERTIES
};

ZEND_GET_MODULE(sentry);


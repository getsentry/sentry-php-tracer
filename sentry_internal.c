#include "php.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"
#include "sentry_internal.h"

void sentry_zval_ptr_dtor_undef(zval *zv) {
    if (!Z_ISUNDEF_P(zv)) {
        zval_ptr_dtor(zv);
        ZVAL_UNDEF(zv);
    }
}

typedef struct {
    zval *object;
    zend_string *name;
} sentry_operation_context;

static bool sentry_call_method_operation(void *context, zval *retval) {
    sentry_operation_context *ctx = context;

    zend_result result = zend_call_method_if_exists(
        Z_OBJ_P(ctx->object),
        ctx->name,
        retval,
        0,
        NULL
    );

    return result == SUCCESS;
}

static bool sentry_read_property_operation(void *context, zval *retval) {
    sentry_operation_context *ctx = context;

    zval property_value;
    ZVAL_UNDEF(&property_value);

    zval *value = zend_read_property_ex(
        Z_OBJCE_P(ctx->object),
        Z_OBJ_P(ctx->object),
        ctx->name,
        true,
        &property_value
    );

    if (value == NULL || Z_ISUNDEF_P(value)) {
        sentry_zval_ptr_dtor_undef(&property_value);
        return false;
    }

    ZVAL_COPY(retval, value);

    sentry_zval_ptr_dtor_undef(&property_value);

    return true;
}

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


typedef bool (*sentry_guarded_operation)(void *context, zval *retval);

static bool sentry_run_internal_call_guarded(
    sentry_guarded_operation operation,
    void *context,
    zval *retval
) {
    ZVAL_UNDEF(retval);

    sentry_exception_state state;
    sentry_exception_isolation_start(&state);

    bool previous = sentry_enter_internal_call();
    bool ok = operation(context, retval);
    sentry_leave_internal_call(previous);

    zend_object *suppressed = sentry_exception_isolation_end(&state);
    if (suppressed != NULL) {
        OBJ_RELEASE(suppressed);

        sentry_zval_ptr_dtor_undef(retval);
        return false;
    }

    if (!ok || Z_ISUNDEF_P(retval)) {
        sentry_zval_ptr_dtor_undef(retval);
        return false;
    }
    return true;
}

void sentry_call_user_function_isolated(
    zval *callback,
    zval *retval,
    uint32_t param_count,
    zval *params
) {
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
}

bool sentry_call_method_guarded(
    zval *object,
    zend_string *method_name,
    zval *retval
) {
    if (Z_TYPE_P(object) != IS_OBJECT) {
        return false;
    }

    sentry_operation_context ctx = {
        object,
        method_name,
    };

    return sentry_run_internal_call_guarded(
        sentry_call_method_operation,
        &ctx,
        retval
    );
}

bool sentry_read_property_guarded(
    zval *object,
    zend_string *property_name,
    zval *retval
) {
    if (Z_TYPE_P(object) != IS_OBJECT) {
        return false;
    }

    sentry_operation_context ctx = {
        object,
        property_name,
    };

    return sentry_run_internal_call_guarded(
        sentry_read_property_operation,
        &ctx,
        retval
    );
}
#ifndef SENTRY_PHP_TRACER_SENTRY_INTERNAL_H
#define SENTRY_PHP_TRACER_SENTRY_INTERNAL_H

#include "php.h"

void sentry_zval_ptr_dtor_undef(zval *zv);

bool sentry_call_method_guarded(
    zval *object,
    zend_string *method_name,
    zval *retval
);

void sentry_call_user_function_isolated(
    zval *callback,
    zval *retval,
    uint32_t param_count,
    zval *params
);

bool sentry_enter_internal_call(void);
void sentry_leave_internal_call(bool previous_in_callback);

bool sentry_read_property_guarded(
      zval *object,
      zend_string *property_name,
      zval *retval
  );

#endif //SENTRY_PHP_TRACER_SENTRY_INTERNAL_H

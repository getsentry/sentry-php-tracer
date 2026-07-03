#ifndef SENTRY_PHP_TRACER_SENTRY_LOG_H
#define SENTRY_PHP_TRACER_SENTRY_LOG_H
#include "php.h"

#define SENTRY_LOG_DEBUG 100
#define SENTRY_LOG_INFO 200
#define SENTRY_LOG_WARNING 300
#define SENTRY_LOG_ERROR 400

typedef struct {
    zend_long line;
    zend_string *name;
    zend_string *file;
    zend_string *message;
} sentry_exception_info;

void sentry_get_exception_info(zend_object *exception, sentry_exception_info *info);

zend_string *sentry_format_exception_log_message(const char *prefix, zend_object *exception);

void sentry_register_log_constants(int module_number);

#endif //SENTRY_PHP_TRACER_SENTRY_LOG_H

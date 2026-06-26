#include "sentry_log.h"
#include <zend_exceptions.h>

void sentry_get_exception_info(zend_object *exception, sentry_exception_info *info) {
    zval rv;
    zend_long line = 0;
    zend_string *file = zend_empty_string;
    zend_string *message = zend_empty_string;

    zend_class_entry *ec = zend_get_exception_base(exception);

    zval *line_zv = zend_read_property(ec, exception, "line", sizeof("line")-1, 1, &rv);
    if (line_zv != NULL && Z_TYPE_P(line_zv) == IS_LONG) {
        line = Z_LVAL_P(line_zv);
    }

    zval *file_zv = zend_read_property(ec, exception, "file", sizeof("file")-1, 1, &rv);
    if (file_zv != NULL && Z_TYPE_P(file_zv) == IS_STRING) {
        file = Z_STR_P(file_zv);
    }

    zval *message_zv = zend_read_property(ec, exception, "message", sizeof("message")-1, 1, &rv);
    if (message_zv != NULL && Z_TYPE_P(message_zv) == IS_STRING) {
        message = Z_STR_P(message_zv);
    }

    info->line = line;
    info->file = file;
    info->message = message;
    info->name = exception->ce->name;
}

zend_string *sentry_format_exception_log_message(const char *prefix, zend_object *exception) {
    sentry_exception_info exception_info;
    sentry_get_exception_info(exception, &exception_info);

    zend_string *message = zend_strpprintf(
        0,
        "%s %s: %s in %s:" ZEND_LONG_FMT,
        prefix,
        ZSTR_VAL(exception_info.name),
        ZSTR_VAL(exception_info.message),
        ZSTR_VAL(exception_info.file),
        exception_info.line
    );

    return message;
}

void sentry_register_log_constants(int module_number) {
    REGISTER_NS_LONG_CONSTANT(
        "Sentry",
        "LOG_DEBUG",
        SENTRY_LOG_DEBUG,
        CONST_CS | CONST_PERSISTENT
    );

    REGISTER_NS_LONG_CONSTANT(
        "Sentry",
        "LOG_INFO",
        SENTRY_LOG_INFO,
        CONST_CS | CONST_PERSISTENT
    );

    REGISTER_NS_LONG_CONSTANT(
        "Sentry",
        "LOG_WARNING",
        SENTRY_LOG_WARNING,
        CONST_CS | CONST_PERSISTENT
    );

    REGISTER_NS_LONG_CONSTANT(
        "Sentry",
        "LOG_ERROR",
        SENTRY_LOG_ERROR,
        CONST_CS | CONST_PERSISTENT
    );
}
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_sentry.h"

PHP_FUNCTION(sentry)
{
    ZEND_PARSE_PARAMETERS_NONE();

    php_printf("The extension %s is loaded and working!\r\n", PHP_SENTRY_EXTNAME);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_sentry, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry sentry_functions[] = {
    PHP_FE(sentry, arginfo_sentry)
    PHP_FE_END
};

zend_module_entry sentry_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_SENTRY_EXTNAME,
    sentry_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    PHP_SENTRY_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SENTRY
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(sentry)
#endif

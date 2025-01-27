#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_sentry.h"
#include "zend_observer.h"

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

PHP_MINIT_FUNCTION(sentry) {
#if defined(ZTS) && defined(COMPILE_DL_SENTRY)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    /* zend_observer_fcall_register(); */

    return SUCCESS;
}

zend_module_entry sentry_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_SENTRY_EXTNAME,
    sentry_functions,
    PHP_MINIT(sentry),          /* PHP_MINIT - Module initialization */
    NULL,                       /* PHP_MSHUTDOWN - Module shutdown */
    NULL,                       /* PHP_RINIT - Request initialization */
    NULL,                       /* PHP_RSHUTDOWN - Request shutdown */
    NULL,                       /* PHP_MINFO - Module info */
    PHP_SENTRY_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SENTRY
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(sentry)
#endif

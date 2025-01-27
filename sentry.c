#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_sentry.h"
#include "sentry_arginfo.h"
#include "zend_observer.h"
#include "zend_closures.h"

PHP_FUNCTION(sentry) {
    ZEND_PARSE_PARAMETERS_NONE();

    php_printf("The extension %s is loaded and working!\r\n", PHP_SENTRY_EXTNAME);
}

PHP_FUNCTION(Sentry_trace) {
    zend_string *class;
    zend_string *function;
    zval *closure = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 4)
        Z_PARAM_STR_OR_NULL(class)
        Z_PARAM_STR(function)
        Z_PARAM_OBJECT_OF_CLASS_OR_NULL(closure, zend_ce_closure)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(add_tracer(class, function, closure));
}

PHP_RINIT_FUNCTION(sentry) {
#if defined(ZTS) && defined(COMPILE_DL_OPENTELEMETRY)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    // tracer_init();

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(sentry) {
    // tracer_cleanup();

    return SUCCESS;
}

PHP_MINIT_FUNCTION(sentry) {
#if defined(ZTS) && defined(COMPILE_DL_SENTRY)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    // zend_observer_fcall_register();

    return SUCCESS;
}

zend_module_entry sentry_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_SENTRY_EXTNAME,
    ext_functions,
    PHP_MINIT(sentry),          /* PHP_MINIT - Module initialization */
    NULL,                       /* PHP_MSHUTDOWN - Module shutdown */
    PHP_RINIT(sentry),          /* PHP_RINIT - Request initialization */
    PHP_RSHUTDOWN(sentry),      /* PHP_RSHUTDOWN - Request shutdown */
    NULL,                       /* PHP_MINFO - Module info */
    PHP_SENTRY_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SENTRY
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(sentry)
#endif

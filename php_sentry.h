#ifndef PHP_SENTRY_H
#define PHP_SENTRY_H

extern zend_module_entry sentry_module_entry;
#define phpext_sentry_ptr &sentry_module_entry

#define PHP_SENTRY_VERSION "1.0"
#define PHP_SENTRY_EXTNAME "sentry"

PHP_FUNCTION(sentry);

# if defined(ZTS) && defined(COMPILE_DL_TEST)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#endif /* PHP_SENTRY_H */

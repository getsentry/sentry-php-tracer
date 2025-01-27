/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: f7b643bda50e8238549b1d7d38f2a9dd9645e616 */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Sentry_trace, 0, 3, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, class, IS_STRING, 1)
	ZEND_ARG_TYPE_INFO(0, function, IS_STRING, 0)
	ZEND_ARG_OBJ_INFO(0, closure, Closure, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_sentry, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_FUNCTION(Sentry_trace);
ZEND_FUNCTION(sentry);

static const zend_function_entry ext_functions[] = {
	ZEND_RAW_FENTRY(ZEND_NS_NAME("Sentry", "trace"), zif_Sentry_trace, arginfo_Sentry_trace, 0, NULL, NULL)
	ZEND_FE(sentry, arginfo_sentry)
	ZEND_FE_END
};

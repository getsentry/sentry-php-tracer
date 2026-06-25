/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 8f9ab3a122f4878e771f49c4114c3703cefba0cd */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Sentry_instrument, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, class_name, IS_STRING, 1)
	ZEND_ARG_TYPE_INFO(0, function_name, IS_STRING, 0)
	ZEND_ARG_VARIADIC_TYPE_INFO(0, metadata, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Sentry_setEndCallback, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

#define arginfo_Sentry_setStartCallback arginfo_Sentry_setEndCallback

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Sentry_Trace___construct, 0, 0, 0)
	ZEND_ARG_VARIADIC_TYPE_INFO(0, metadata, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_FUNCTION(Sentry_instrument);
ZEND_FUNCTION(Sentry_setEndCallback);
ZEND_FUNCTION(Sentry_setStartCallback);
ZEND_METHOD(Sentry_Trace, __construct);

static const zend_function_entry ext_functions[] = {
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY(ZEND_NS_NAME("Sentry", "instrument"), zif_Sentry_instrument, arginfo_Sentry_instrument, 0, NULL, NULL)
#else
	ZEND_RAW_FENTRY(ZEND_NS_NAME("Sentry", "instrument"), zif_Sentry_instrument, arginfo_Sentry_instrument, 0)
#endif
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY(ZEND_NS_NAME("Sentry", "setEndCallback"), zif_Sentry_setEndCallback, arginfo_Sentry_setEndCallback, 0, NULL, NULL)
#else
	ZEND_RAW_FENTRY(ZEND_NS_NAME("Sentry", "setEndCallback"), zif_Sentry_setEndCallback, arginfo_Sentry_setEndCallback, 0)
#endif
#if (PHP_VERSION_ID >= 80400)
	ZEND_RAW_FENTRY(ZEND_NS_NAME("Sentry", "setStartCallback"), zif_Sentry_setStartCallback, arginfo_Sentry_setStartCallback, 0, NULL, NULL)
#else
	ZEND_RAW_FENTRY(ZEND_NS_NAME("Sentry", "setStartCallback"), zif_Sentry_setStartCallback, arginfo_Sentry_setStartCallback, 0)
#endif
	ZEND_FE_END
};

static const zend_function_entry class_Sentry_Trace_methods[] = {
	ZEND_ME(Sentry_Trace, __construct, arginfo_class_Sentry_Trace___construct, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

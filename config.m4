AC_DEFINE(HAVE_SENTRY, 1, [Whether you have Sentry])
PHP_NEW_EXTENSION(sentry, sentry.c sentry_log.c, shared)

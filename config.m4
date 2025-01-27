PHP_ARG_ENABLE(sentry, whether to enable Sentry support,
    [ --enable-sentry    Enable Sentry support])
if test "$PHP_SENTRY" = "yes"; then
    AC_DEFINE(HAVE_SENTRY, 1, [Whether you have Sentry])
    PHP_NEW_EXTENSION(Sentry, sentry.c, $ext_shared)
fi
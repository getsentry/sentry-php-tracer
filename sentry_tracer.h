#ifndef SENTRY_TRACER_H
#define SENTRY_TRACER_H

void sentry_tracer_init(INIT_FUNC_ARGS);
void sentry_tracer_globals_init(void);
void sentry_tracer_globals_cleanup(void);

bool add_tracer(zend_string *class, zend_string *function, zval *closure);

#endif // SENTRY_TRACER_H

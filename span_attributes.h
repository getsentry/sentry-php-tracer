#ifndef SENTRY_SPAN_ATTRIBUTES_H
#define SENTRY_SPAN_ATTRIBUTES_H

#include "php.h"

typedef struct {
    zval metadata;
    // contains sentry_span_attribute_rule for one instrumented function
    HashTable span_attributes;
    zval span_attributes_definition;
    bool span_attributes_resolved;
} sentry_instrumentation;

typedef enum {
    SENTRY_SPAN_ATTRIBUTE_ACCESS_DIRECT,
    SENTRY_SPAN_ATTRIBUTE_ACCESS_PROPERTY,
    SENTRY_SPAN_ATTRIBUTE_ACCESS_METHOD
} sentry_span_attribute_access_kind;

typedef struct {
    // name of the parameter
    zend_string *name;
    // index of the parameter
    uint32_t param_index;
    // the path to invoke on the parameter to get the final data. null means it will use
    // the value as-is
    zend_string *path;
    // the type of access. Can be either direct (if scalar), or property or method
    sentry_span_attribute_access_kind access_kind;
} sentry_span_attribute_rule;

void sentry_span_attribute_rule_dtor(zval *zv);

void sentry_instrumentation_dtor(zval *zv);

bool sentry_is_span_attributes_arg(zend_string *name, zval *value);

void sentry_add_span_attribute_rules(
    HashTable *rules,
    zend_function *func,
    zval *span_attributes
);

bool sentry_resolve_param_index(
    zend_function *func,
    zend_string *param_name,
    uint32_t *param_index
);

void sentry_apply_span_attribute_rules(
    sentry_instrumentation *instrumentation,
    zend_execute_data *execute_data,
    zval *metadata
);

#endif

#include "php.h"
#include "Zend/zend_interfaces.h"
#include "span_attributes.h"
#include "sentry_internal.h"

static bool sentry_path_is_method_call(zend_string *path) {
    size_t length = ZSTR_LEN(path);

    if (length <= 2) {
        return false;
    }

    return ZSTR_VAL(path)[length - 2] == '('
        && ZSTR_VAL(path)[length - 1] == ')';
}

static zend_string *sentry_path_method_name(zend_string *path) {
    return zend_string_init(
        ZSTR_VAL(path),
        ZSTR_LEN(path) - 2,
        0
    );
}

void sentry_span_attribute_rule_dtor(zval *zv) {
    sentry_span_attribute_rule *rule = Z_PTR_P(zv);

    zend_string_release(rule->name);

    if (rule->path != NULL) {
        zend_string_release(rule->path);
    }

    efree(rule);
}

void sentry_instrumentation_dtor(zval *zv) {
    sentry_instrumentation *instrumentation = Z_PTR_P(zv);

    zval_ptr_dtor(&instrumentation->metadata);
    zend_hash_destroy(&instrumentation->span_attributes);

    efree(instrumentation);
}

bool sentry_is_span_attributes_arg(zend_string *name, zval *value) {
    return name != NULL
        && Z_TYPE_P(value) == IS_ARRAY
        && zend_string_equals_literal(name, "spanAttributes");
}

bool sentry_resolve_param_index(
    zend_function *func,
    zend_string *param_name,
    uint32_t *param_index
) {
    if (func->common.arg_info == NULL) {
        return false;
    }

    for (uint32_t i = 0; i < func->common.num_args; i++) {
        zend_arg_info *arg_info = &func->common.arg_info[i];

        if (arg_info->name != NULL && zend_string_equals(arg_info->name, param_name)) {
            *param_index = i;
            return true;
        }
    }

    return false;
}

void sentry_add_span_attribute_rules(
    HashTable *rules,
    zend_function *func,
    zval *span_attributes
) {
    zend_string *attribute_name;
    zval *definition;

    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(span_attributes), attribute_name, definition) {
        if (attribute_name == NULL) {
            continue;
        }

        zval *param_name = NULL;
        zval *path = NULL;

        if (Z_TYPE_P(definition) == IS_STRING) {
            param_name = definition;
        } else if (Z_TYPE_P(definition) == IS_ARRAY) {
            param_name = zend_hash_index_find(Z_ARRVAL_P(definition), 0);
            path = zend_hash_index_find(Z_ARRVAL_P(definition), 1);
        } else {
            continue;
        }

        if (param_name == NULL || Z_TYPE_P(param_name) != IS_STRING) {
            continue;
        }

        if (path != NULL && Z_TYPE_P(path) != IS_STRING) {
            continue;
        }

        uint32_t param_index = 0;
        if (!sentry_resolve_param_index(func, Z_STR_P(param_name), &param_index)) {
            continue;
        }

        sentry_span_attribute_rule *rule = emalloc(sizeof(sentry_span_attribute_rule));
        rule->name = zend_string_copy(attribute_name);
        rule->param_index = param_index;

        if (path == NULL) {
            rule->access_kind = SENTRY_SPAN_ATTRIBUTE_ACCESS_DIRECT;
            rule->path = NULL;
        } else if (sentry_path_is_method_call(Z_STR_P(path))) {
            rule->access_kind = SENTRY_SPAN_ATTRIBUTE_ACCESS_METHOD;
            rule->path = sentry_path_method_name(Z_STR_P(path));
        } else {
            rule->access_kind = SENTRY_SPAN_ATTRIBUTE_ACCESS_PROPERTY;
            rule->path = zend_string_copy(Z_STR_P(path));
        }

        zval rule_zv;
        ZVAL_PTR(&rule_zv, rule);
        if (zend_hash_next_index_insert(rules, &rule_zv) == NULL) {
            sentry_span_attribute_rule_dtor(&rule_zv);
        }
    } ZEND_HASH_FOREACH_END();
}

static bool sentry_copy_span_attribute_value(zval *source, zval *dest) {
    ZVAL_DEREF(source);

    switch (Z_TYPE_P(source)) {
        case IS_NULL:
        case IS_FALSE:
        case IS_TRUE:
        case IS_LONG:
        case IS_DOUBLE:
            ZVAL_COPY_VALUE(dest, source);
            return true;
        case IS_STRING:
            ZVAL_COPY(dest, source);
            return true;
        default:
            return false;
    }
}


static bool sentry_evaluate_method_path(
    zval *root,
    zend_string *method_name,
    zval *result
) {
    if (Z_TYPE_P(root) != IS_OBJECT) {
        return false;
    }

    zval retval;
    ZVAL_UNDEF(&retval);

    bool called = sentry_call_method_guarded(root, method_name, &retval);

    if (!called) {
        return false;
    }

    if (!sentry_copy_span_attribute_value(&retval, result)) {
        zval_ptr_dtor(&retval);
        return false;
    }

    zval_ptr_dtor(&retval);
    return true;
}

static bool sentry_evaluate_property_path(
    zval *root,
    zend_string *path,
    zval *result
) {
    zval property_value;
    ZVAL_UNDEF(&property_value);

    if (!sentry_read_property_guarded(root, path, &property_value)) {
        return false;
    }

    bool copied = sentry_copy_span_attribute_value(&property_value, result);
    zval_ptr_dtor(&property_value);

    return copied;
}

void sentry_apply_span_attribute_rules(
    sentry_instrumentation *instrumentation,
    zend_execute_data *execute_data,
    zval *metadata
) {
    if (zend_hash_num_elements(&instrumentation->span_attributes) == 0) {
        return;
    }

    SEPARATE_ARRAY(metadata);

    zval *rule_zv;

    ZEND_HASH_FOREACH_VAL(&instrumentation->span_attributes, rule_zv) {
        sentry_span_attribute_rule *rule = Z_PTR_P(rule_zv);

        if (rule->param_index >= ZEND_CALL_NUM_ARGS(execute_data)) {
            continue;
        }

        zval *argument = ZEND_CALL_ARG(execute_data, rule->param_index + 1);

        zval copied_value;
        bool copied;

        switch (rule->access_kind) {
            case SENTRY_SPAN_ATTRIBUTE_ACCESS_DIRECT:
                copied = sentry_copy_span_attribute_value(argument, &copied_value);
                break;
            case SENTRY_SPAN_ATTRIBUTE_ACCESS_METHOD:
                copied = sentry_evaluate_method_path(argument, rule->path, &copied_value);
                break;

            case SENTRY_SPAN_ATTRIBUTE_ACCESS_PROPERTY:
                copied = sentry_evaluate_property_path(argument, rule->path, &copied_value);
                break;

            default:
                copied = false;
        }

        if (!copied) {
            continue;
        }

        zend_hash_update(Z_ARRVAL_P(metadata), rule->name, &copied_value);
    } ZEND_HASH_FOREACH_END();
}


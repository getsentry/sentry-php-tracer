<?php

/**
 * @generate-function-entries
 * @generate-legacy-arginfo 80000
 */

namespace Sentry {
    function instrument(
        ?string $class_name,
        string $function_name,
        array $extra_metadata = []
    ): bool {}

    function setEndCallback(callable $callback): bool {}

    function setStartCallback(callable $callback): bool {}

    #[\Attribute(\Attribute::TARGET_FUNCTION | \Attribute::TARGET_METHOD)]
    final class Trace {
        public function __construct(array $metadata = []) {}
    }
}

<?php

/**
 * @generate-function-entries
 * @generate-legacy-arginfo 80000
 *
 */

namespace Sentry {
    function instrument(
        ?string $class_name,
        string $function_name,
        mixed ...$metadata
    ): bool {}

    /**
     * @phpstan-param callable(array{name: string, start_time: float, end_time: float, duration: float, metadata: array<string, mixed>}, mixed): mixed $callback
     */
    function setEndCallback(callable $callback): bool {}

    /**
     * @phpstan-param callable(array{name: string, start_time: float, metadata: array<string, mixed>}): mixed $callback
     */
    function setStartCallback(callable $callback): bool {}

    #[\Attribute(\Attribute::TARGET_FUNCTION | \Attribute::TARGET_METHOD)]
    final class Trace {
        public function __construct(mixed ...$metadata) {}
    }
}

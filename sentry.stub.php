<?php

/**
 * @generate-function-entries
 * @generate-legacy-arginfo 80000
 *
 * @phpstan-type StartData array{name: string, start_time: float, metadata: array<string, mixed>}
 * @phpstan-type EndData array{name: string, start_time: float, end_time: float, duration: float, metadata: array<string, mixed>}
 */

namespace Sentry {
    function instrument(
        ?string $class_name,
        string $function_name,
        array $extra_metadata = []
    ): bool {}

    /** @param callable(EndData, mixed): mixed $callback */
    function setEndCallback(callable $callback): bool {}

    /** @param callable(StartData): mixed $callback */
    function setStartCallback(callable $callback): bool {}

    #[\Attribute(\Attribute::TARGET_FUNCTION | \Attribute::TARGET_METHOD)]
    final class Trace {
        public function __construct(array $metadata = []) {}
    }
}

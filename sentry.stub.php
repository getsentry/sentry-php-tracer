<?php

/**
 * @generate-function-entries
 * @generate-legacy-arginfo 80000
 *
 */

namespace Sentry {
    const LOG_DEBUG = 100;
    const LOG_INFO = 200;
    const LOG_WARNING = 300;
    const LOG_ERROR = 400;

    function instrument(
        ?string $className,
        string $functionName,
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

    /**
     * @phpstan-param callable(int, string): mixed $callback
     */
    function setLogCallback(callable $callback): bool {}

    #[\Attribute(\Attribute::TARGET_FUNCTION | \Attribute::TARGET_METHOD)]
    final class Trace {
        public function __construct(mixed ...$metadata) {}
    }
}

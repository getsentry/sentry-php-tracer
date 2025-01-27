<?php

/**
 * @generate-class-entries 
 * @generate-function-entries
 */

namespace Sentry {
    function trace(
        string|null $class,
        string $function,
        \Closure $closure
    ): bool {}
}

namespace {
    function sentry(): void {}
}

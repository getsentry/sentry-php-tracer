--TEST--
Tests that explicitly typed instrument options retain tolerant runtime behavior.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented() {
}

\Sentry\setLogCallback(static function (int $level, string $message) {
    echo $level . ':' . $message . PHP_EOL;
});

\Sentry\setEndCallback(static function (array $data) {
    echo 'attributes=' . $data['metadata']['attributes'] . PHP_EOL;
});

\Sentry\instrument(
    function: 'test_instrumented',
    preprocessing: 'not-callable',
    postprocessing: 42,
    attributes: 'kept-as-metadata',
);

test_instrumented();

?>
--EXPECT--
300:Sentry instrumentation argument "preprocessing" for 'test_instrumented' is not a valid callback and was ignored.
300:Sentry instrumentation argument "postprocessing" for 'test_instrumented' is not a valid callback and was ignored.
attributes=kept-as-metadata

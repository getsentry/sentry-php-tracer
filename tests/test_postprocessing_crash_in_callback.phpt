--TEST--
Tests that throwing an exception in the postprocessing callback does not crash the application.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string $foo, int $bar) {
    return "return value";
}

\Sentry\setEndCallback(static function (array $data) {
    if (isset($data['metadata']['return'])) {
        echo "Return: " . $data['metadata']['return'] . PHP_EOL;
    } else {
        echo "No return value" . PHP_EOL;
    }
}); 

\Sentry\instrument('test_instrumented', postprocessing: static function (string $return) {
    throw new \RuntimeException("oh no");
});

test_instrumented("hello", 42);

?>
--EXPECTF--
No return value
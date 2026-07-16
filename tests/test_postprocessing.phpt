--TEST--
Tests that the postprocessing callback captures the return value.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string $foo, int $bar) {
    return "return value";
}

\Sentry\setStartCallback(static function (array $data) {
    if (isset($data['metadata']['return'])) {
        echo "Return: " . $data['metadata']['return'] . PHP_EOL;
    } else {
        echo "No return value" . PHP_EOL;
    }
});

\Sentry\setEndCallback(static function (array $data) {
    echo "Return: " . $data['metadata']['return'] . PHP_EOL;
}); 

\Sentry\instrument('test_instrumented', postprocessing: static function (string $return) {
    return [
        'return' => $return,
    ];
});

test_instrumented("hello", 42);

?>
--EXPECTF--
No return value
Return: return value
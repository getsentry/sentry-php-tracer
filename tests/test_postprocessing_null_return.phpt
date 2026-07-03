--TEST--
Tests that the postprocessing callback captures void return as null
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string $foo, int $bar): ?string {
    return null;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Return: " . $data['metadata']['return'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', postprocessing: static function ($return) {
    return [
        'return' => \gettype($return),
    ];
});

test_instrumented("hello", 42);

?>
--EXPECTF--
Return: NULL
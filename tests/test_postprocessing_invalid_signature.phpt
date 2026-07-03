--TEST--
Tests that the postprocessing callback will not crash the application if the types in the signature are wrong.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string $foo, int $bar): string {
    return "this is a returned string";
}

\Sentry\setEndCallback(static function (array $data) {
    if (isset($data['metadata']['return'])) {
        echo "Return: " . $data['metadata']['return'] . PHP_EOL;    
    } else {
        echo "no return value";
    }
}); 

\Sentry\instrument(null, 'test_instrumented', postprocessing: static function (int $return) {
    return [
        'return' => \gettype($return),
    ];
});

test_instrumented("hello", 42);

?>
--EXPECTF--
no return value
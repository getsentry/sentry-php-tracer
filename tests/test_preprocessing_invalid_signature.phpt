--TEST--
Tests that the preprocessing callback will not crash the application if the types in the signature are wrong.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string $foo, int $bar): string {
    return "this is a returned string";
}

\Sentry\setStartCallback(static function (array $data) {
    echo "Foo: " . ($data['metadata']['foo'] ?? 'No Foo') . PHP_EOL;
    echo "Bar: " . ($data['metadata']['bar'] ?? 'No Bar'). PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', postprocessing: static function (int $foo, float $bar) {
    return [
        'return' => \gettype($return),
    ];
});

test_instrumented("hello", 42);

?>
--EXPECTF--
Foo: No Foo
Bar: No Bar
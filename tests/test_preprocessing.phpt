--TEST--
Tests that the preprocessing callback with capture call parameters and convert them.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string $foo, int $bar) {
    
}

\Sentry\setStartCallback(static function (array $data) {
    echo "Foo: " . $data['metadata']['foo'] . PHP_EOL;
    echo "Bar: " . $data['metadata']['bar'] . PHP_EOL; 
});

\Sentry\setEndCallback(static function (array $data) {
    echo "Foo: " . $data['metadata']['foo'] . PHP_EOL;
    echo "Bar: " . $data['metadata']['bar'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', preprocessing: static function (string $foo, int $bar) {
    return [
        'foo' => $foo,
        'bar' => $bar
    ];
});

test_instrumented("hello", 42);

?>
--EXPECTF--
Foo: hello
Bar: 42
Foo: hello
Bar: 42
--TEST--
Tests that spanAttributes can invoke magic methods and get the correct return value.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    public function __get(string $name) {
        return $name . 'Magic';
    }
}

function test_instrumented(A $param) {
    return $param->foo;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'foo']
]);
test_instrumented((new A()));

?>
--EXPECTF--
Description: fooMagic
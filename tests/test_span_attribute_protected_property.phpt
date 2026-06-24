--TEST--
Tests that spanAttributes can specify paths to access a protected class property.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    protected $example = "bar";
}

function test_instrumented(A $param) {
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'example']
]);
test_instrumented((new A()));

?>
--EXPECTF--
Description: bar
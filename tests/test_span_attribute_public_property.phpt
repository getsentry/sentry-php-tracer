--TEST--
Tests that spanAttributes can specify paths to access a public class property.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    public $example = "bar";
}

function test_instrumented(A $param) {
    return $param->example;
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
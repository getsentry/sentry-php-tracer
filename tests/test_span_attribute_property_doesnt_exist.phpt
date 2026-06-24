--TEST--
Tests that spanAttributes can specify paths to access a class property that doesn't exist.
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
    if (isset($data['metadata']['description'])) {
        echo "Description: " . $data['metadata']['description'] . PHP_EOL;
    } else {
        echo "doesn't exist";
    }
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'foo']
]);
test_instrumented((new A()));

?>
--EXPECTF--
doesn't exist
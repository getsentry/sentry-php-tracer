--TEST--
Tests that spanAttributes will produce null for uninitialized properties.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    private string $x;
}

function test_instrumented(A $param) {
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    $description = $data['metadata']['description'] ?: "null";
    echo "Description: " . $description . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'x']
]);

test_instrumented(new A());

?>
--EXPECTF--
Description: null
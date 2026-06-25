--TEST--
Tests that spanAttributes can specify paths to invoke methods without params.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    public function getFoo(): string {
        return "foo";
    }
}

function test_instrumented(A $param) {
    return $param->getFoo();
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'getFoo()']
]);
test_instrumented((new A()));

?>
--EXPECTF--
Description: foo
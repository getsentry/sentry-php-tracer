--TEST--
Tests that multiple span attributes can be extracted from parameters
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    public function getFoo(): string {
        return "foo";
    }
}

function test_instrumented(A $param, string $test) {
    return $param->getFoo();
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
    echo "Origin: " . $data['metadata']['origin'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'getFoo()'],
    'origin' => ['test']
]);
test_instrumented((new A()), 'sentry');

?>
--EXPECTF--
Description: foo
Origin: sentry
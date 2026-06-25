--TEST--
Tests that spanAttributes can specify paths to invoke methods that doesn't exist.
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
    if (isset($data['metadata']['description'])) {
        echo "Description: " . $data['metadata']['description'] . PHP_EOL;
    } else {
        echo "doesn't exist";
    }
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'getBar()']
]);
test_instrumented((new A()));

?>
--EXPECTF--
doesn't exist
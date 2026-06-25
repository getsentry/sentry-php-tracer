--TEST--
Tests that spanAttributes can specify paths to invoke methods that throws an exception.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    public function getFoo(): string {
        throw new \RuntimeException("oh no");
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
    'description' => ['param', 'getFoo()']
]);
try {
test_instrumented((new A()));
} catch (\Throwable $t) {

}

?>
--EXPECTF--
doesn't exist
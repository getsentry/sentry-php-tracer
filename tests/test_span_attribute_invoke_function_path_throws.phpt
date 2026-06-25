--TEST--
Tests that spanAttributes can specify paths to invoke methods does not throw itself but the spanAttributes invocation throws.
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
    echo "test_instrumented invoked" . PHP_EOL;
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    if (isset($data['metadata']['description'])) {
        echo "Description: " . $data['metadata']['description'] . PHP_EOL;
    } else {
        echo "doesn't exist" . PHP_EOL;
    }
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'getFoo()']
]);
try {
test_instrumented((new A()));
echo "after call" . PHP_EOL;
} catch (\Throwable $t) {

}

?>
--EXPECTF--
test_instrumented invoked
doesn't exist
after call
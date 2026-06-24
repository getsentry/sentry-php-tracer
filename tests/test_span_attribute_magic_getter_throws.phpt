--TEST--
Tests that spanAttributes can invoke magic methods that throw an exception which does not crash the user application.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    public function __get(string $name) {
        throw new \RuntimeException("magic throw");
    }
}

function test_instrumented(A $param) {
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    if (isset($data['metadata']['description'])) {
        echo "Description exists";    
    } else {
        echo "Description does not exist";
    }
});

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'foo']
]);
test_instrumented((new A()));

?>
--EXPECTF--
Description does not exist
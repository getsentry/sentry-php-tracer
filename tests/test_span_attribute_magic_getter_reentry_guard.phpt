--TEST--
Tests that spanAttributes can invoke magic methods which do not trigger callbacks.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    public function __get(string $name) {
        return $this->instrumented();
    }
    
    public function instrumented() {
        return "instrumented";
    }
}

function test_instrumented(A $param) {
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . ($data['metadata']['description'] ?? "Empty"). PHP_EOL;
});

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param', 'foo']
]);
\Sentry\instrument("A", "instrumented");
test_instrumented((new A()));

?>
--EXPECTF--
Description: instrumented
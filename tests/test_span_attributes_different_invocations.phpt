--TEST--
Tests that invoking it with different paths to different objects will get the correct result
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    private $foo;

    public function __construct(string $foo) {
        $this->foo = $foo;
    }
    
    public function getFoo() {
        return $this->foo;
    }
}

function test_instrumented(A $test) {
    return $test;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['test', 'getFoo()']
]);
test_instrumented(new A("foo"));
test_instrumented(new A("bar"));
test_instrumented(new A("baz"));

?>
--EXPECTF--
Description: foo
Description: bar
Description: baz
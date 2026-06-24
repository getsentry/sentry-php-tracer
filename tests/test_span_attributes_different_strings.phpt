--TEST--
Tests that invoking multiple times with different strings will produce the expected outcome
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string $test) {
    return $test;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['test']
]);
test_instrumented("foo");
test_instrumented("bar");
test_instrumented("baz");

?>
--EXPECTF--
Description: foo
Description: bar
Description: baz
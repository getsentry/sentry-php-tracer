--TEST--
Tests that parameters can be captured by using the simplified syntax.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(int $test) {
    return $test;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => 'test'
]);
test_instrumented(123);

?>
--EXPECTF--
Description: 123
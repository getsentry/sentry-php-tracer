--TEST--
Tests that passing spanAttribute to \Sentry\Instrument will capture the value if it's a float.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(float $test) {
    return $test;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['test']
]);
test_instrumented(123.456);

?>
--EXPECTF--
Description: 123.456
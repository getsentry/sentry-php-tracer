--TEST--
Tests that passing spanAttribute to \Sentry\Instrument will capture the value if it's true.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(bool $test) {
    return $test;
}

\Sentry\setEndCallback(static function (array $data) {
    if ($data['metadata']['description'] === true) {
        echo "Description: true" . PHP_EOL;
    }
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['test']
]);
test_instrumented(true);

?>
--EXPECTF--
Description: true
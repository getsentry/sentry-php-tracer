--TEST--
Tests that passing spanAttribute to \Sentry\Instrument will capture the value if it's false.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(bool $test) {
    return $test;
}

\Sentry\setEndCallback(static function (array $data) {
    if ($data['metadata']['description'] === false) {
        echo "Description: false" . PHP_EOL;
    }
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['test']
]);
test_instrumented(false);

?>
--EXPECTF--
Description: false
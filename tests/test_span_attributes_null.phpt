--TEST--
Tests that passing spanAttribute to \Sentry\Instrument will capture the value if it's null.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented($test) {
    return $test;
}

\Sentry\setEndCallback(static function (array $data) {
    if ($data['metadata']['description'] === null) {
        echo "Description: null" . PHP_EOL;
    }
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['test']
]);
test_instrumented(null);

?>
--EXPECTF--
Description: null
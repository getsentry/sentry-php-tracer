--TEST--
Tests that functions that are loaded after instrumentation calls can still capture parameters as span attributes.
--EXTENSIONS--
sentry
--FILE--
<?php


\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

var_dump(\Sentry\instrument(null, 'instrumented_function', spanAttributes: ['description' => 'test']));

require __DIR__ . '/fixtures/functions.php';

instrumented_function("instrumented function");

?>
--EXPECTF--
bool(true)
Description: instrumented function
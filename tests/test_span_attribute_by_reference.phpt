--TEST--
Tests that spanAttributes can capture params by reference.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string &$param) {
    return $param;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', spanAttributes: [
    'description' => ['param']
]);

$x = "foo";
test_instrumented($x);

?>
--EXPECTF--
Description: foo
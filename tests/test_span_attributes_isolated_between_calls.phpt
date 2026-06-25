--TEST--
Tests that invoking a function with default null will not cache the value.
--EXTENSIONS--
sentry
--FILE--
<?php

function f($x = null) {}

\Sentry\setEndCallback(static function (array $data) {
    echo "Description: " . ($data['metadata']['description'] ?? 'null') . PHP_EOL;
}); 

\Sentry\instrument(null, 'f', spanAttributes: ['description' => ['x']]);

f('test');
f();

?>
--EXPECTF--
Description: test
Description: null
--TEST--
Tests that registering the first time for instrumentation returns true and false on subsequent calls to `\Sentry\instrument`.
--EXTENSIONS--
sentry
--FILE--
<?php

function work() {
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
}); 

$result = \Sentry\instrument(null, 'work');
echo "First result: " . ($result ? "true" : "false") . PHP_EOL;
$result = \Sentry\instrument(null, 'work');
echo "Second result: " . ($result ? "true" : "false") . PHP_EOL;
work();

?>
--EXPECTF--
First result: true
Second result: false
Name: work
Duration: %f
--TEST--
Tests that instrumenting a non existient function will not cause any crashes or errors.
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

\Sentry\instrument(null, 'working');
work();

?>
--EXPECTF--
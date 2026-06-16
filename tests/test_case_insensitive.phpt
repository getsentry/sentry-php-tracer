--TEST--
Tests that instrumentation can be registered case insensitive and will still capture the correctly cased name.
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

\Sentry\instrument(null, 'WoRk');
work();

?>
--EXPECTF--
Name: work
Duration: %f
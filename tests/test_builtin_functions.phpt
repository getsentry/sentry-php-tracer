--TEST--
Tests a regular function instrumented by instrument
--EXTENSIONS--
sentry
--FILE--
<?php

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'trim');
trim("   abcde   ");

?>
--EXPECTF--
Name: trim
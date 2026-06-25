--TEST--
Tests that built-in functions are ignored by instrument.
--EXTENSIONS--
sentry
--FILE--
<?php

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
}); 

echo \Sentry\instrument(null, 'trim') ? "true" : "false";
trim("   abcde   ");

?>
--EXPECTF--
false
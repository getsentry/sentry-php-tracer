--TEST--
Tests the value of the registered constants by the extension. Also proves their existence.
--EXTENSIONS--
sentry
--FILE--
<?php

echo Sentry\LOG_DEBUG . PHP_EOL;
echo Sentry\LOG_INFO . PHP_EOL;
echo Sentry\LOG_WARNING . PHP_EOL;
echo Sentry\LOG_ERROR . PHP_EOL;

?>
--EXPECTF--
100
200
300
400

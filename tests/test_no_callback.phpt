--TEST--
Tests that if no callbacks are registered, the application will just work normally without any interference.
--EXTENSIONS--
sentry
--FILE--
<?php

function work() {
    return 10;
}

$result = \Sentry\instrument(null, 'work');
work();

?>
--EXPECTF--

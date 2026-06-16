--TEST--
Tests that exceptions thrown in the start callback will be swallowed silently and not break userland applications.
--EXTENSIONS--
sentry
--FILE--
<?php

function work() {
    return 10;
}

\Sentry\setStartCallback(static function (array $data) {
    throw new \RuntimeException("Does not break out");
}); 

\Sentry\instrument(null, 'work', ['sentry.op' => 'test']);
work();

?>
--EXPECTF--
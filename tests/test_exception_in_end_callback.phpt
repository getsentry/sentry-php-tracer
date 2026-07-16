--TEST--
Tests that exceptions thrown in the end callback will be swallowed silently and not break userland applications.
--EXTENSIONS--
sentry
--FILE--
<?php

function work() {
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    throw new \RuntimeException("Does not break out");
}); 

\Sentry\instrument('work', attributes: ['sentry.op' => 'test']);
work();

?>
--EXPECTF--
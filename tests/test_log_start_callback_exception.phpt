--TEST--
Tests that a log message will be produced if the start callback throws.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented() {
    return 10;
}

\Sentry\setStartCallback(static function(array $data) {
    throw new \RuntimeException("start callback crashed!");
});

\Sentry\setLogCallback(static function(int $level, string $message) {
    echo $level . ":" . $message . PHP_EOL;
});

\Sentry\instrument('test_instrumented', attributes: []);
test_instrumented();

?>
--EXPECTF--
400:Sentry start callback threw an exception and was ignored. RuntimeException: start callback crashed! in %s:%d

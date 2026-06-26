--TEST--
Tests that a log message will be produced if the end callback throws.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented() {
    return 10;
}

\Sentry\setEndCallback(static function(array $data) {
    throw new \RuntimeException("end callback crashed!");
});

\Sentry\setLogCallback(static function(int $level, string $message) {
    echo $level . ":" . $message . PHP_EOL;
});

\Sentry\instrument(null, 'test_instrumented', []);
test_instrumented();

?>
--EXPECTF--
400:Sentry end callback threw an exception and was ignored. RuntimeException: end callback crashed! in %s:%d

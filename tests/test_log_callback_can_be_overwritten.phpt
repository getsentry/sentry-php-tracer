--TEST--
Tests that setLogCallback can be overwritten and will use the newer one.
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

\Sentry\instrument(null, 'test_instrumented', []);
test_instrumented();

\Sentry\setLogCallback(static function(int $level, string $message) {
    echo "Overwritten callback";
});

test_instrumented();

?>
--EXPECTF--
400:Sentry start callback threw an exception and was ignored. RuntimeException: start callback crashed! in %s:%d
Overwritten callback
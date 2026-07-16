--TEST--
Tests that exceptions in the log callback will not trigger logs again and prevent infinite recursion
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented() {
    return 10;
}

\Sentry\setEndCallback(static function(array $data) {
    throw new \RuntimeException("oh no");
});

\Sentry\setLogCallback(static function(int $level, string $message) {
    echo $level . ":" . $message . PHP_EOL;
    throw new \RuntimeException("log callback crashed");
});

\Sentry\instrument('test_instrumented', attributes: []);
test_instrumented();

?>
--EXPECTF--
400:Sentry end callback threw an exception and was ignored. RuntimeException: oh no in %s:%d

--TEST--
Tests that the preprocessing callback that throws an exception doesn't crash the application
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string $foo, int $bar) {
    
}

\Sentry\setStartCallback(static function (array $data) {
    echo "start callback" . PHP_EOL;
});

\Sentry\setEndCallback(static function (array $data) {
    echo "end callback" . PHP_EOL;
}); 

\Sentry\instrument('test_instrumented', preprocessing: static function (string $foo, int $bar) {
    throw new \RuntimeException("Oh no");
});

test_instrumented("hello", 42);

?>
--EXPECTF--
start callback
end callback
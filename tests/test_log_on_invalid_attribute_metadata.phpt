--TEST--
Tests that invalid attributes with names will produce logs.
--EXTENSIONS--
sentry
--FILE--
<?php

#[\Sentry\Trace(foo: UnknownConstant::Unknown)]
function test_instrumented() {
    return 10;
}

\Sentry\setLogCallback(static function(int $level, string $message) {
    echo $level . ":" . $message . PHP_EOL;
});

test_instrumented();

?>
--EXPECTF--
300:Sentry Trace attribute argument 'foo' on 'test_instrumented' could not be evaluated and was ignored.

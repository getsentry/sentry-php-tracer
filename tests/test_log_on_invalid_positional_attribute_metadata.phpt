--TEST--
Tests that invalid positional attributes will produce logs.
--EXTENSIONS--
sentry
--FILE--
<?php

#[\Sentry\Trace(UnknownConstant::Unknown)]
function test_instrumented() {
    return 10;
}

#[\Sentry\Trace([], UnknownConstant::Unknown)]
function test() {
    return 10;    
}

\Sentry\setLogCallback(static function(int $level, string $message) {
    echo $level . ":" . $message . PHP_EOL;
});

test_instrumented();
test();

?>
--EXPECTF--
300:Sentry Trace attribute argument #1 on 'test_instrumented' could not be evaluated and was ignored.
300:Sentry Trace attribute argument #2 on 'test' could not be evaluated and was ignored.

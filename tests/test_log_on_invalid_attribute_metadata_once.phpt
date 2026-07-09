--TEST--
Tests that an invalid attribute argument is warned about once per request, not on every call.
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
test_instrumented();

echo "Done" . PHP_EOL;

?>
--EXPECT--
300:Sentry Trace attribute argument 'foo' on 'test_instrumented' could not be evaluated and was ignored.
Done

--TEST--
Tests that duplicate calls to instrument emits logs.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented() {
    $result = 0;
    for($i = 0; $i < 1000; $i++) {
      $result += $i;
    }
    
    return $result;
}

\Sentry\setLogCallback(static function(int $level, string $message) {
    echo $level . ":" . $message . PHP_EOL;
});

var_dump(\Sentry\instrument(null, 'test_instrumented', []));
var_dump(\Sentry\instrument(null, 'test_instrumented', []));

?>
--EXPECTF--
bool(true)
100:Sentry instrumentation target 'test_instrumented' is already registered and was ignored.
bool(false)
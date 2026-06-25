--TEST--
Tests that the Trace annotation can use named arguments and they get passed into metadata
--EXTENSIONS--
sentry
--FILE--
<?php

#[\Sentry\Trace(op: "foo", description: "bar", custom: "oh no")]
function test_instrumented() {
    $result = 0;
    for($i = 0; $i < 1000; $i++) {
      $result += $i;
    }
    
    return $result;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
    echo "OP: " . $data['metadata']['op'] . PHP_EOL;
    echo "Custom: " . $data['metadata']['custom'] . PHP_EOL;
});

test_instrumented();

?>
--EXPECTF--
Name: test_instrumented
Duration: %f
OP: foo
Custom: oh no
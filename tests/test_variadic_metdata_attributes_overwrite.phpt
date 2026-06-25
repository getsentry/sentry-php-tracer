--TEST--
Tests that the Trace attribute using name arguments has special handling for the attributes named argument
--EXTENSIONS--
sentry
--FILE--
<?php

#[\Sentry\Trace(op: "foo", description: "bar", custom: "oh no", attributes: ["custom" => "overwrite", "test" => "example"])]
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
    echo "Test: " . $data['metadata']['test'] . PHP_EOL;
    echo "Custom: " . $data['metadata']['custom'] . PHP_EOL;
});

test_instrumented();

?>
--EXPECTF--
Name: test_instrumented
Duration: %f
OP: foo
Test: example
Custom: overwrite
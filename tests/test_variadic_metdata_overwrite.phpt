--TEST--
Tests that the attributes named parameter has special handling and overwrites previously declared params.
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

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
    echo "OP: " . $data['metadata']['op'] . PHP_EOL;
    echo "Test: " . $data['metadata']['test'] . PHP_EOL;
    echo "Custom: " . $data['metadata']['custom'] . PHP_EOL;
});

\Sentry\instrument(null, "test_instrumented", op: "foo", description: "bar", custom: "oh no", attributes: ["custom" => "abc", "test" => "example"]);
test_instrumented();

?>
--EXPECTF--
Name: test_instrumented
Duration: %f
OP: foo
Test: example
Custom: abc
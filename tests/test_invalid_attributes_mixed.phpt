--TEST--
Tests that using invalid and valid attributes only extract the valid ones
--EXTENSIONS--
sentry
--FILE--
<?php

#[\Sentry\Trace(name: FakeConstant::Fake, description: "Test")]
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
    echo "Description: " . $data['metadata']['description'] . PHP_EOL;
}); 

test_instrumented();

?>
--EXPECTF--
Name: test_instrumented
Duration: %f
Description: Test
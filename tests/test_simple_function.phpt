--TEST--
Tests a regular function instrumented by instrument
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
}); 

\Sentry\instrument(null, 'test_instrumented', []);
test_instrumented();

?>
--EXPECTF--
Name: test_instrumented
Duration: %f
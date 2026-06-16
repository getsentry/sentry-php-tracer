--TEST--
Tests that any value returned by the start callback is returned to the end callback as second parameter.
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

\Sentry\setStartCallback(static function (array $data) {
    return "This value is passed to the end callback as second param"; 
});

\Sentry\setEndCallback(static function (array $data, string $returnValue) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
    echo $returnValue . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented');
test_instrumented();

?>
--EXPECTF--
Name: test_instrumented
Duration: %f
This value is passed to the end callback as second param
--TEST--
Tests that metadata declared in the Trace attribute will be passed to the callbacks.
--EXTENSIONS--
sentry
--FILE--
<?php

#[\Sentry\Trace(['sentry.op' => 'test'])]
function test_instrumented() {
    $result = 0;
    for($i = 0; $i < 1000; $i++) {
      $result += $i;
    }
    
    return $result;
}

\Sentry\setStartCallback(static function (array $data) {
    echo "Start name: " . $data['name'] . PHP_EOL;
    echo "Start metadata: " . ($data['metadata']['sentry.op'] ?? 'invalid') . PHP_EOL;
});

\Sentry\setEndCallback(static function (array $data) {
    echo "End name: " . $data['name'] . PHP_EOL;
    echo "End duration: " . $data['duration'] . PHP_EOL;
    echo "End metadata: " . ($data['metadata']['sentry.op'] ?? 'invalid') . PHP_EOL;
}); 

test_instrumented();

?>
--EXPECTF--
Start name: test_instrumented
Start metadata: test
End name: test_instrumented
End duration: %f
End metadata: test
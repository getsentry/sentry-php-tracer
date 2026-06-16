--TEST--
Tests that metadata declared in `instrument` will be passed to the callbacks.
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
    echo "Start name: " . $data['name'] . PHP_EOL;
    echo "Start Metadata: " . ($data['metadata']['sentry.op'] ?? 'invalid') . PHP_EOL;
});

\Sentry\setEndCallback(static function (array $data) {
    echo "End name: " . $data['name'] . PHP_EOL;
    echo "End duration: " . $data['duration'] . PHP_EOL;
    echo "End metadata: " . ($data['metadata']['sentry.op'] ?? 'invalid') . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented', ['sentry.op' => 'test']);
test_instrumented();

?>
--EXPECTF--
Start name: test_instrumented
Start Metadata: test
End name: test_instrumented
End duration: %f
End metadata: test
--TEST--
Tests that numeric array keys in attributes are ignored
--EXTENSIONS--
sentry
--FILE--
<?php

#[\Sentry\Trace(attributes: [0 => 'test', 1 => 'abc', 'test' => 'example'])]
function test_instrumented() {
    $result = 0;
    for($i = 0; $i < 1000; $i++) {
      $result += $i;
    }
    
    return $result;
}

\Sentry\setEndCallback(static function (array $data) {
    foreach ($data['metadata'] as $key => $value) {
        echo $key . ": " . $value . PHP_EOL;
    }
});

test_instrumented();

?>
--EXPECTF--
test: example
--TEST--
Tests that numeric array keys in attributes are ignored
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
    foreach ($data['metadata'] as $key => $value) {
        echo $key . ": " . $value . PHP_EOL;
    }
});

\Sentry\instrument(null, "test_instrumented", attributes: [0 => 'test', 1 => 'abc', 'test' => 'example']);
test_instrumented();

?>
--EXPECTF--
test: example
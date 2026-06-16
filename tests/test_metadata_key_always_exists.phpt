--TEST--
Tests that the 'metadata' key in data in the end callback is always populated and never null (Function)
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented() {
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    if (!isset($data['metadata'])) {
        echo "metadata array is not set" . PHP_EOL;
    } else if ($data['metadata'] === null) {
        echo "metadata array is null" . PHP_EOL;
    } else {
        echo "[]" . PHP_EOL;
    }
}); 

\Sentry\instrument(null, 'test_instrumented');
test_instrumented();

?>
--EXPECTF--
[]
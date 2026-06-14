--TEST--
Tests that exceptions thrown in instrumented functions do not interfere with the end callback.
--EXTENSIONS--
sentry
--FILE--
<?php

function work() {
    throw new \RuntimeException("boom");
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
    echo "Metadata: " . ($data['metadata']['sentry.op'] ?? 'invalid') . PHP_EOL;
}); 

\Sentry\instrument(null, 'work', ['sentry.op' => 'test']);
try {
    work();
} catch (Throwable $t) {

}

?>
--EXPECTF--
Name: work
Duration: %f
Metadata: test
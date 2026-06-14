--TEST--
Tests that calling instrumented functions from a callback will not instrument them again to prevent
infinite recursion.
--EXTENSIONS--
sentry
--FILE--
<?php

function work() {
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
    echo "Metadata: " . ($data['metadata']['sentry.op'] ?? 'invalid') . PHP_EOL;
    work();
}); 

\Sentry\instrument(null, 'work', ['sentry.op' => 'test']);
work();

?>
--EXPECTF--
Name: work
Duration: %f
Metadata: test
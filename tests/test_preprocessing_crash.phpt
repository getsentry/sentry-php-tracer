--TEST--
Tests that the postprocessing callback is not invoked when the instrumented function throws an exception.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented(string $foo, int $bar) {
    throw new \RuntimeException("oh no");
}

\Sentry\setEndCallback(static function (array $data) {
    if (isset($data['metadata']['return'])) {
        echo "Return: " . $data['metadata']['return'] . PHP_EOL;  
    } else {
        echo 'Postprocessing callback did not run' . PHP_EOL;
    }
}); 

\Sentry\instrument(null, 'test_instrumented', postprocessing: static function () {
    return ['return' => 'return'];
});

try {
test_instrumented("hello", 42);
} catch (\Throwable $t) {
    echo "Exception caught";
}

?>
--EXPECTF--
Postprocessing callback did not run
Exception caught
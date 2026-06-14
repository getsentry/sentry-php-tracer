--TEST--
Tests that if a function and the callback throw, the callback exception is not leaked and the original exception
can be recovered.
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
    throw new \RuntimeException("callback boom");
}); 

\Sentry\instrument(null, 'work');
try {
    work();
} catch (Throwable $throwable) {
    echo $throwable->getMessage() . PHP_EOL;
}

?>
--EXPECTF--
Name: work
Duration: %f
boom
--TEST--
Tests that re-throwing different exceptions properly populates the 'exception' key.
--EXTENSIONS--
sentry
--FILE--
<?php

class CustomException extends Exception {}

function test_throw() {
    throw new CustomException("Oh no"); 
}

function test_rethrow() {
    try {
        test_throw();
    } catch (Throwable $t) {
        echo "Exception caught" . PHP_EOL;
    }
}

\Sentry\setEndCallback(static function (array $data) {
    $exception = $data['exception'];
    if ($exception !== null) {
        echo $exception->getMessage() . PHP_EOL;
        echo get_class($exception) . PHP_EOL;
    } else {
        echo 'No exception here' . PHP_EOL;
    }
}); 

\Sentry\instrument(null, 'test_throw');
\Sentry\instrument(null, 'test_rethrow');
try {
test_rethrow();
} catch (Throwable $t) {

}

?>
--EXPECTF--
Oh no
CustomException
Exception caught
No exception here
--TEST--
Tests that catching an exception in a function will make 'exception' be null in the end callback
--EXTENSIONS--
sentry
--FILE--
<?php

class CustomException extends Exception {}

class TestException extends Exception {}

function test_throw() {
    throw new CustomException("Oh no"); 
}

function test_rethrow() {
    try {
        test_throw();
    } catch (Throwable $t) {
        throw new TestException("throw different exception");
    }
}

\Sentry\setEndCallback(static function (array $data) {
    $exception = $data['exception'];
    echo $exception->getMessage() . PHP_EOL;
    echo get_class($exception) . PHP_EOL;
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
throw different exception
TestException
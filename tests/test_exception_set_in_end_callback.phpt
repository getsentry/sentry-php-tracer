--TEST--
Tests that a thrown exception is captured and stored under the 'exception' key in the end callback
--EXTENSIONS--
sentry
--FILE--
<?php

class CustomException extends Exception {

}

function test_throw() {
    throw new CustomException("Oh no"); 
}

\Sentry\setEndCallback(static function (array $data) {
    $exception = $data['exception'];
    echo $exception->getMessage() . PHP_EOL;
    echo get_class($exception) . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_throw');
try {
test_throw();
} catch (Throwable $t) {

}

?>
--EXPECTF--
Oh no
CustomException
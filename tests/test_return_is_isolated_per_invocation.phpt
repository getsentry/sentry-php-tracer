--TEST--
Tests that the value returned from the start callback is properly isolated and handled to the correct end callback.
--EXTENSIONS--
sentry
--FILE--
<?php

#[\Sentry\Trace(['test' => 'test1'])]
function foo() {
    bar();
}

#[\Sentry\Trace(['test' => 'test2'])]
function bar() {
    baz();
}

#[\Sentry\Trace(['test' => 'test3'])]
function baz() {
    return 30;
}

\Sentry\setStartCallback(static function(array $data) {
    return $data['metadata']['test']; 
});

\Sentry\setEndCallback(static function (array $data, string $userData) {
    echo $userData . PHP_EOL;
}); 

foo();

?>
--EXPECTF--
test3
test2
test1
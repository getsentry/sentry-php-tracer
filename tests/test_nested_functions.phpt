--TEST--
Tests that nested calls will invoke the callbacks in the correct order.
--EXTENSIONS--
sentry
--FILE--
<?php

function work1() {
    return work2();
}

function work2() {
    return work3();
}

function work3() {
    return 100;
}

\Sentry\setStartCallback(static function (array $data) {
    echo "Start name: " . $data['name'] . PHP_EOL;
});

\Sentry\setEndCallback(static function (array $data) {
    echo "End name: " . $data['name'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'work1');
\Sentry\instrument(null, 'work2');
\Sentry\instrument(null, 'work3');

work1();

?>
--EXPECTF--
Start name: work1
Start name: work2
Start name: work3
End name: work3
End name: work2
End name: work1
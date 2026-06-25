--TEST--
Tests that some fields are always present in the start and end callback and also assert their type.
--EXTENSIONS--
sentry
--FILE--
<?php

function test_instrumented() {
    return 10;
}

\Sentry\setStartCallback(static function (array $data) {
    echo 'Start callback' . PHP_EOL;
    echo 'start_time: ' . get_debug_type($data['start_time'])  . PHP_EOL;
    echo 'name: ' . get_debug_type($data['name']) . PHP_EOL;
});

\Sentry\setEndCallback(static function (array $data) {
    echo 'End callback' . PHP_EOL;
    echo 'start_time: ' . get_debug_type($data['start_time']) . PHP_EOL;
    echo 'name: ' . get_debug_type($data['name']) . PHP_EOL;
    echo 'duration: ' . get_debug_type($data['duration']) . PHP_EOL;
    echo 'end_time: ' . get_debug_type($data['end_time']) . PHP_EOL;
    echo 'metadata: ' . get_debug_type($data['metadata']) . PHP_EOL;
}); 

\Sentry\instrument(null, 'test_instrumented');
test_instrumented();

?>
--EXPECTF--
Start callback
start_time: float
name: string
End callback
start_time: float
name: string
duration: float
end_time: float
metadata: array

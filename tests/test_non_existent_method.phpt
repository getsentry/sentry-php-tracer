--TEST--
Tests that instrumenting a non existent method will not cause any crashes or errors.
--EXTENSIONS--
sentry
--FILE--
<?php

class Foo {
    public function work() {
        return 10;
    }
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
}); 

\Sentry\instrument(null, 'work');
(new Foo())->work();

?>
--EXPECTF--
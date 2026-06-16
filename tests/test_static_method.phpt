--TEST--
Tests that static functions can be instrumented using `\Sentry\instrument`
--EXTENSIONS--
sentry
--FILE--
<?php

class Foo {
    public static function work() {
        return 10;
    }
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
}); 

\Sentry\instrument("Foo", 'work', []);
Foo::work();

?>
--EXPECTF--
Name: Foo::work
Duration: %f
--TEST--
Tests a method can be instrumented using `\Sentry\instrument`
--EXTENSIONS--
sentry
--FILE--
<?php

class Foo {
    public function work() {
        return 10 + 2000;
    }
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
}); 

\Sentry\instrument("Foo", 'work', []);
(new Foo())->work();

?>
--EXPECTF--
Name: Foo::work
Duration: %f
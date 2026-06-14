--TEST--
Tests a method can be instrumented using `#[\Sentry\Trace]` attribute.
--EXTENSIONS--
sentry
--FILE--
<?php

class Foo {
    #[\Sentry\Trace]
    public function work() {
        return 10 + 2000;
    }
}

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
}); 

(new Foo())->work();

?>
--EXPECTF--
Name: Foo::work
Duration: %f
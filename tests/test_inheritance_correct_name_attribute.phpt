--TEST--
Tests that inherited functions that are not overwritten can be traced using `\Sentry\Trace` attribute.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    #[\Sentry\Trace]
    public function work() {
        return 10;
    }
}

class B extends A {

}

\Sentry\setEndCallback(static function (array $data) {
    echo "Name: " . $data['name'] . PHP_EOL;
    echo "Duration: " . $data['duration'] . PHP_EOL;
}); 

(new B())->work();
(new A())->work();

?>
--EXPECTF--
Name: B::work
Duration: %f
Name: A::work
Duration: %f
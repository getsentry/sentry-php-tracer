--TEST--
Tests that inherited functions that are not overwritten can be traced using `\Sentry\instrument`.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
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

\Sentry\instrument("B", "work");

(new B())->work();
(new A())->work();

?>
--EXPECTF--
Name: B::work
Duration: %f
Name: A::work
Duration: %f
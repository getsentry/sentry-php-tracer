--TEST--
Tests that registering an inherited method via the subclass after it was already traced invalidates the shared cache entry.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    #[\Sentry\Trace(['source' => 'attribute'])]
    public function work() {
        return 10;
    }
}

class B extends A {

}

\Sentry\setEndCallback(static function (array $data) {
    echo $data['name'] . " source: " . $data['metadata']['source'] . PHP_EOL;
});

(new B())->work();

\Sentry\instrument("B", "work", ['source' => 'registration']);

(new B())->work();
(new A())->work();

?>
--EXPECT--
B::work source: attribute
B::work source: registration
A::work source: registration

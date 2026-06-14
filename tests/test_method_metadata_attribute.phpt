--TEST--
Tests that metadata declared in the Trace attribute will be passed to the callbacks.
--EXTENSIONS--
sentry
--FILE--
<?php

class Foo {
    #[\Sentry\Trace(['sentry.op' => 'test'])]
    public function work() {
        return 10 + 2000;
    }
}

\Sentry\setStartCallback(static function (array $data) {
    echo "Start name: " . $data['name'] . PHP_EOL;
    echo "Start metadata: " . ($data['metadata']['sentry.op'] ?? 'invalid') . PHP_EOL;
});

\Sentry\setEndCallback(static function (array $data) {
    echo "End name: " . $data['name'] . PHP_EOL;
    echo "End duration: " . $data['duration'] . PHP_EOL;
    echo "End metadata: " . ($data['metadata']['sentry.op'] ?? 'invalid') . PHP_EOL;
}); 

(new Foo())->work();

?>
--EXPECTF--
Start name: Foo::work
Start metadata: test
End name: Foo::work
End duration: %f
End metadata: test
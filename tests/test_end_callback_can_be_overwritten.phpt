--TEST--
Tests that the end callback can be updated by calling setEndCallback again.
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
    echo "First callback" . PHP_EOL;
});
\Sentry\setEndCallback(static function (array $data) {
    echo "Second callback" . PHP_EOL;
});

\Sentry\instrument("Foo", 'work', []);
(new Foo())->work();

?>
--EXPECTF--
Second callback
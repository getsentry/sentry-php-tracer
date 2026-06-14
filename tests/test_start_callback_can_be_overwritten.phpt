--TEST--
Tests that the start callback can be updated by calling setStartCallback again.
--EXTENSIONS--
sentry
--FILE--
<?php

class Foo {
    public function work() {
        return 10 + 2000;
    }
}

\Sentry\setStartCallback(static function (array $data) {
    echo "First callback" . PHP_EOL;
});
\Sentry\setStartCallback(static function (array $data) {
    echo "Second callback" . PHP_EOL;
});

\Sentry\instrument("Foo", 'work', []);
(new Foo())->work();

?>
--EXPECTF--
Second callback
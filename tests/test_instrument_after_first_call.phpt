--TEST--
Tests that a registration added after an attribute-traced function was already called is picked up on the next call.
--EXTENSIONS--
sentry
--FILE--
<?php

#[\Sentry\Trace(['source' => 'attribute'])]
function work() {
    return 10;
}

\Sentry\setEndCallback(static function (array $data) {
    echo $data['name'] . " source: " . $data['metadata']['source'] . PHP_EOL;
});

work();

\Sentry\instrument('work', attributes: ['source' => 'registration']);

work();

?>
--EXPECT--
work source: attribute
work source: registration

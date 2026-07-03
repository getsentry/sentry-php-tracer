--TEST--
Tests that the preprocessing callback captures object parameters.
--EXTENSIONS--
sentry
--FILE--
<?php

class A {
    private $x;
    
    public function __construct($x) {
        $this->x = $x;
    }
    
   public  function getX()
    {
        return $this->x;
    }
}

function test_instrumented(A $a) {
    return $a->getX();
}

\Sentry\setStartCallback(static function (array $data) {
    if (isset($data['metadata']['param'])) {
        echo "Object: " . $data['metadata']['param'] . PHP_EOL;
    } else {
        echo "No object value" . PHP_EOL;
    }
});

\Sentry\setEndCallback(static function (array $data) {
    if (isset($data['metadata']['param'])) {
        echo "Object: " . $data['metadata']['param'] . PHP_EOL;
    } else {
        echo "No object value" . PHP_EOL;
    }
}); 

\Sentry\instrument(null, 'test_instrumented', preprocessing: static function (A $param) {
    return [
        'param' => $param->getX(),
    ];
});

test_instrumented(new A('object parameter'));

?>
--EXPECTF--
Object: object parameter
Object: object parameter
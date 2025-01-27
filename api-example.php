<?php

Sentry\trace(
    class: User::class,
    function: 'getRecords',
    clousre: fn (Span $span) => $span->setAtttributes([
        'key_one' => 1,
        'key_two' => 1,
    ]),
);

// and/or

Sentry\trace(
    class: User::class,
    function: 'getRecords',
    pre: function () {
        $span = Sentry::makeSpan(
            name: 'spanName',
            attributes: ['key_one' => 1],
        );
    },
    post: function (Span $span) {
        $span->finish();
    },
);

// and

#[Sentry\trace(name: "spanName", attributes: ['key_one' => 1])]
function getRecords() {

}

# sentry-php-tracer

> [!CAUTION]
> Work in progress. Expect API changes while not 1.0

Sentry extension to enable automatic instrumentation of methods and functions.

Methods/functions can be instrumented by either declaring the #[\Sentry\Trace] attribute on them
or registering them with `\Sentry\instrument("MyClass", "myFunction)`.

The extension is meant to only provide telemetry information for methods/functions, such as `duration` or `start_time`.
Creating spans and maintaining a proper span will be done in the SDK to minimize the number of required updates
for the extension itself.

## Callbacks

The extension offers two callbacks, one before a function is executed and one after execution.

Data returned in the startCallback will be passed back as second parameter of the endCallback.
This provides a way to pass data between the callbacks without a custom storage.

Each callback gets a data array as first parameter with the following keys:

| Key        | When      | Description                                      |
|------------|-----------|--------------------------------------------------|
| start_time | start,end | The start time as float timestamp                |
| end_time   | end       | The end time as float timestamp                  |
| duration   | end       | The duration in float milliseconds               |
| name       | start,end | The name of the function/method                  |  
| metadata   | start,end | The array with the metadata specified, see below |  

### Example

```php
\Sentry\setStartCallback(static function (array $data) {
    return ['spanId' => generateSpanId()];
});

\Sentry\setEndCallback(static function (array $data, $userData) {
    setTelemetryData([
        'name' => $data['name'],
        'duration' => $data['duration'],
        'spanId' => $userData['spanId'];
    ]);
});

```

## Attributes/Metadata

Metadata/attributes can be provided in the attribute and the instrument function.

Attribute:
`#[\Sentry\Trace(['my-attribute' => 'test', 'other' => 'foo'])]`

Function:
`\Sentry\instrument("MyClass", "myFunction", ['my-attribute' => 'test', 'other' => 'foo']`

## Build the extension

```
phpize
./configure
make
make install
```

To verify the extension is loaded, run

```
php -m | grep -i sentry
```

## Generate _arginfo.h

```
make <file-name>_arginfo.h
```

## Contributing

### Development environment

To build PHP locally, you'll need a variety of dependencies.
For macOS, you may use [`brew`](https://brew.sh/) to install them.

```
brew install autoconf
brew install bison
brew install libiconv
brew install re2c
brew install pkg-config
```

Update your `PATH`.

```
export PATH="/opt/homebrew/opt/bison/bin:$PATH"
export PATH="/opt/homebrew/opt/libiconv/bin:$PATH"
```

Next, clone the PHP source.

```
git clone git@github.com:php/php-src.git
```

Checkout the release branch you want to compile the extension against,
for the time being, we'll use 8.2.0.

```
git checkout -b php-8.2.0 PHP-8.2.0
```

Create a `php.ini`.

```
touch ~/php-bin/DEBUG/etc
```

```
date.timezone=GMT
max_execution_time=30
memory_limit=128M

error_reporting=E_ALL | E_STRICT
display_errors=1
log_errors=1

extension=sentry.so
```

We can now build an NTS DEBUG version of PHP.

```
./buildconf

./configure --enable-debug \
  --with-iconv=$(brew --prefix libiconv) \
  --prefix=$HOME/php-bin/DEBUG \
  --with-config-file-path=$HOME/php-bin/DEBUG/etc

make -j8 (Depending on the amount of logical cores of your CPU)
make install
```

Make sure the compiled PHP version is first in your `PATH`.

```
export PATH="$HOME/php-bin/DEBUG/bin:$PATH"
```

You can now build the extension as mentioned above.

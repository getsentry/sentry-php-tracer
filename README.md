# sentry-php-tracer

This is work in progress.

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

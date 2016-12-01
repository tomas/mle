# eon

A clever little console text editor. Written in C, forked out of [mle](https://github.com/adsr/mle).

## Building

    $ git clone https://github.com/tomas/eon.git
    $ cd eon
    $ git submodule update --init --recursive
    $ sudo apt-get install libpcre3-dev # or yum install pcre-devel, brew install pcre-dev, etc
    $ make

You can run `make eon_static` instead to build a static binary.


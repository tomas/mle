# eon

A clever little console text editor. Written in C, forked out of [mle](https://github.com/adsr/mle).

## Building

Install deps first:

    $ apt install cmake libpcre # or brew install / apk add

Clone the repo and initialize submodules:

    $ git clone https://github.com/tomas/eon.git
    $ cd eon
    $ git submodule update --init --recursive

And off you go!

    $ make

You can run `make eon_static` instead to build a static binary.


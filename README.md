# ToxSuite
__NOT READY FOR PRODUCTION__

ToxSuite is a C library to develop services and tools for Tox.  
See [tox.chat](https://tox.chat) for information about Tox in general.   

[![Coverity Scan Build Status](https://scan.coverity.com/projects/8730/badge.svg)](https://scan.coverity.com/projects/bignaux-toxsuite)
[![Build Status](https://travis-ci.org/bignaux/ToxSuite.svg?branch=master)](https://travis-ci.org/bignaux/ToxSuite)

---

## Features

* modular : code is split to different files
* [fopencookie] : write message to friend with standard stream 
* [libsodium] : decryption/encryption support of toxfile
* [libsndfile] : audio streaming/recording
* [ylog] : low-level logging system
* [captcha] : can generate captcha to deal with spammer

The ToxSuite comes with two tools :

* Suit : a Tox bot performing ala Skype calltest.
* Toxdatatool : a tool for processing toxfile.

## Installation

Currently, ToxSuite uses qmake to generate Makefile and is only tested on GNU/Linux 64 bits.

### Build Toxdatatool

```sh
$ qmake -config toxdatatool
$ make
```

### Build Suit

```sh
$ qmake -config suite
$ make
```

# Suit

Suit bot on Tox. No configuration file yet, configure it hacking the source code.


# Toxdatatool

Tox profile management.

## Usage

```
$ ./toxdatatool -?
Usage: toxdatatool <toxfile>
-s, --passphrase=<passphrase>     Use passphrase to encrypt/decrypt toxfile.
-x, --export=<tox.keys>           Export friends's tox publickey into file.
-i, --import=<tox.keys>           Import friends from tox publickey file.
-r, --remove=<expire.keys>        Remove friends from tox publickey file.
-u, --purge=<delay in second>     Purge friends toxfile not seen since <delay>
-p, --print                       Print content of toxfile (after modification if any).
-v, --verbose                     Be verbose

Help options:
-?, --help                        Show this help message
--usage                       Display brief usage message
```

   [fopencookie]: <http://man7.org/linux/man-pages/man3/fopencookie.3.html>
   [libsodium]: <https://github.com/jedisct1/libsodium>
   [libsndfile]: <http://www.mega-nerd.com/libsndfile>
   [captcha]: <https://github.com/ITikhonov/captcha>
   [jsmn]: <https://github.com/zserge/jsmn>
   [ylog]: <https://dev.yorhel.nl/ylib> 
  
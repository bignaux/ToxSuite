# ToxSuite
__NOT READY FOR PRODUCTION__

ToxSuite is a C library.

### Features

* modular
* write message to friend with standard stream thanks to fopencookie()
* decryption/encryption support of toxfile
* [captcha] : can generate captcha to deal with spammer
* [libsndfile] : audio streaming/recording

The ToxSuite comes with two tools :

* Suit : a Tox bot performing ala Skype calltest.
* Toxdatatool : a tool for processing toxfile.

### Installation

```sh
$ qmake -config toxdatatool
$ make
```
```sh
$ qmake -config suite
$ make
```

### Usage

```sh
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

   [libsndfile]: <http://www.mega-nerd.com/libsndfile>
   [captcha]: <https://github.com/ITikhonov/captcha>
   [jsmn]: <https://github.com/zserge/jsmn>
  
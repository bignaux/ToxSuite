QMAKE_CFLAGS = -std=gnu99 -Wall -Wno-unused-parameter -Wno-unused-result -Wformat-security
# -Wno-unused

#TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
CONFIG -= captcha

# link with gcc instead of g++
QMAKE_LINK = $$QMAKE_LINK_C
#QMAKE_LFLAGS += -Wl,--as-needed

unix: CONFIG += link_pkgconfig ## enable the PKGCONFIG feature
unix: PKGCONFIG += libsodium
unix: PKGCONFIG += libtoxcore

VERSION    = 0.0.1
GIT_VERSION = $$system(git describe --abbrev=8 --dirty --always --tags)
DEFINES += GIT_VERSION=\\\"$$GIT_VERSION\\\"
DEFINES += VERSION=\\\"$$VERSION\\\"

# retroshare remove this :
DEFINES += BE_DEBUG_DECODE
DEFINES += BE_DEBUG

# dirty lib, disabled.
captcha {
    captcha.target = .buildfile
    captcha.commands = make -f src/captcha/Makefile
    QMAKE_EXTRA_TARGETS += captcha
    PRE_TARGETDEPS += .buildfile
    LIBS += src/captcha/libcaptcha.o
    DEFINES += CAPTCHA
}


# on container :
# $ ldd --version
# ldd (Ubuntu EGLIBC 2.15-0ubuntu10.13) 2.15
# clock_gettime() : Link with -lrt (only for glibc versions before 2.17).
TRAVIS = $$(TRAVIS)
!isEmpty(TRAVIS) {
    LIBS += -lrt
    # workaround to use static toxcore lib build against a recent glibc
    # The pb occurs in this case :
    #  * you're using old glibc < 2.17 2
    #  * you don't use toxcore/network.c at all.
    #  * you're not using clock_gettime() in your software
    QMAKE_LFLAGS += -Wl,--no-as-needed
}

suit {
    TARGET = suit
    HEADERS += \
        src/call.h \
        src/command.h \
        src/suit.h \
        src/jsmn/jsmn.h \
        src/file.h \
        src/friend.h \
        src/list.h \
        src/toxdata.h \
        src/ylog/ylog.h \
        src/unused.h \
        src/timespec.h \
        src/misc.h \
        src/callbacks.h\
        src/conf.h\
        src/fileop.h\
        src/filesend.h\
        src/bencode/bencode.h \
        src/tsfiles.h

    SOURCES += \
        src/call.c \
        src/command.c \
        src/suit.c \
        src/jsmn/jsmn.c \
        src/file.c \
        src/friend.c \
        src/toxdata.c \
        src/ylog/ylog.c \
        src/timespec.c \
        src/misc.c \
        src/conf.c \
        src/fileop.c \
        src/filesend.c \
        src/callbacks.c\
        src/bencode/bencode.c \
        src/tsfiles.c

    unix: PKGCONFIG += sndfile
    unix: PKGCONFIG += libtoxav
}

toxdatatool {
  TARGET = toxdatatool
  LIBS += -lpopt
  unix:TEMPLATE = app
  CONFIG -= staticlib
  CONFIG += static
  HEADERS += \
    src/friend.h \
    src/list.h \
    src/toxdata.h \
    src/ylog/ylog.h \
    src/misc.h
  SOURCES += \
    src/toxdatatool.c \
    src/friend.c \
    src/toxdata.c \
    src/ylog/ylog.c \
    src/misc.c
}

#toxdatatool.path = /usr/bin
#suit.path = /usr/bin/
#INSTALLS += suit toxdatatool
#QMAKE_EXTRA_TARGETS += suit toxdatatool

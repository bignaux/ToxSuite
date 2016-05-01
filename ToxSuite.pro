QMAKE_CFLAGS = -std=gnu99 -Wno-unused-parameter -Wno-unused-result -Wformat-security
# -Wno-unused

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
CONFIG -= captcha

unix: CONFIG += link_pkgconfig ## enable the PKGCONFIG feature
unix: PKGCONFIG += libsodium
unix: PKGCONFIG += libtoxcore

VERSION    = 0.0.1
GIT_VERSION = $$system(git describe --abbrev=8 --dirty --always --tags)
DEFINES += GIT_VERSION=\\\"$$GIT_VERSION\\\"
DEFINES += VERSION=\\\"$$VERSION\\\"


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
TRAVIS = $$TRAVIS
equals(TRAVIS, true) {
    LIBS += -lrt --as-needed
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
        src/misc.h
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
        src/misc.c

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

#INSTALLS += suit

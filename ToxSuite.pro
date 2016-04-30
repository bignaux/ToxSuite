QMAKE_CFLAGS = -std=gnu99 -Wno-unused-parameter -Wno-unused-result
# -Wno-unused

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += libsodium
unix: PKGCONFIG += libtoxcore

GIT_VERSION = $$system(git describe --abbrev=8 --dirty --always --tags)
DEFINES += GIT_VERSION=\\\"$$GIT_VERSION\\\"

captcha.target = .buildfile
captcha.commands = make -f src/captcha/Makefile
QMAKE_EXTRA_TARGETS += captcha
PRE_TARGETDEPS += .buildfile


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
    LIBS += src/captcha/libcaptcha.o
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
LIBS += src/captcha/libcaptcha.o
}


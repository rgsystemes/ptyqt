# ******************************************************************************
# COPYRIGHT (c) Since 2008 - by RG System Corporation.
# This software has been provided pursuant to a License Agreement
# containing restrictions on its use.  This software contains
# valuable trade secrets and proprietary information of
# RG System Corporation and is protected by law. It may
# not be copied or distributed in any form or medium, disclosed
# to third parties, reverse engineered or used in any manner not
# provided for in said License Agreement except with the prior
# written authorization from RG System Corporation.
# ******************************************************************************

TEMPLATE = lib

CONFIG += qt
CONFIG += staticlib

TARGET = ptyqt

INCLUDEPATH += .

win32 {
    HEADERS += \
        core/ptyqt.h \
        core/iptyprocess.h \
        core/winptyprocess.h \
        core/conptyprocess.h

    SOURCES += \
        core/ptyqt.cpp \
        core/winptyprocess.cpp \
        core/conptyprocess.cpp

    LIBS += -lwsock32 -lws2_32 -lcrypt32 -liphlpapi -lnetapi32 -lversion -lwinmm -luserenv
}

unix:!macx {
    HEADERS += \
        core/ptyqt.h \
        core/iptyprocess.h \
        core/unixptyprocess.h

    SOURCES += \
        core/ptyqt.cpp \
        core/unixptyprocess.cpp

    LIBS += -lpthread -ldl -static-libstdc++
}

macx {
    HEADERS += \
        core/ptyqt.h \
        core/iptyprocess.h \
        core/unixptyprocess.h

    SOURCES += \
        core/ptyqt.cpp \
        core/unixptyprocess.cpp

    LIBS += \
        -framework Security \
        -framework AppKit \
        -framework CoreFoundation \
        -framework IOKit \
        -framework CoreGraphics \
        -framework CFNetwork \
        -framework CoreText \
        -framework Carbon \
        -framework CoreServices \
        -framework ApplicationServices \
        -framework SystemConfiguration
}

INCLUDEPATH += /usr/include /usr/local/include
LIBS += -lpcre2-8 -lssl -lcrypto -lbz2 -lz -ldouble-conversion

QT += core network

include(../../supv.pri)
DEFINES -= QT_NO_CAST_FROM_ASCII

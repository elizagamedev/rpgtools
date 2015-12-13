TEMPLATE = app
CONFIG -= app_bundle qt
CONFIG += console link_pkgconfig

PKGCONFIG += libpng zlib
DEFINES += UCONV
unix:PKGCONFIG += icu-uc

INCLUDEPATH += common

SOURCES += rpgpatcher/main.cpp \
    rpgpatcher/lcf.cpp \
    rpgpatcher/common.cpp \
    common/bitmap.cpp \
    common/os.cpp \
    common/util.cpp \
    rpgpatcher/patch.cpp \
    rpgpatcher/tr.cpp \
    rpgpatcher/unpatch.cpp

HEADERS += \
    rpgpatcher/lcf.h \
    rpgpatcher/common.h \
    common/bitmap.h \
    common/os.h \
    common/util.h \
    rpgpatcher/tr.h \
    rpgpatcher/patch.h \
    rpgpatcher/unpatch.h

win32:RC_ICONS += common/icon.ico

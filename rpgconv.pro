TEMPLATE = app
CONFIG -= app_bundle qt
CONFIG += console link_pkgconfig

PKGCONFIG += libpng zlib
DEFINES += UCONV
unix:PKGCONFIG += icu-uc

INCLUDEPATH += common

SOURCES += rpgconv/main.cpp \
    common/os.cpp \
    common/util.cpp \
    rpgconv/wolf.cpp \
    rpgconv/rgssa1.cpp \
    rpgconv/rgssa3.cpp \
    rpgconv/rgssa.cpp \
    common/bitmap.cpp

HEADERS += \
    common/os.h \
    common/util.h \
    rpgconv/rgssa.h \
    common/bitmap.h

win32:RC_ICONS += common/icon.ico

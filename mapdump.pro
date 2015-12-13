TEMPLATE = app
CONFIG -= app_bundle qt
CONFIG += console link_pkgconfig

PKGCONFIG += liblcf libpng zlib

INCLUDEPATH += common

SOURCES += \
    common/bitmap.cpp \
    common/os.cpp \
    mapdump/main.cpp \
    common/util.cpp

HEADERS += \
    common/bitmap.h \
    common/os.h \
    common/util.h

win32:RC_ICONS += common/icon.ico

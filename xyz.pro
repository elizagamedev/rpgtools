TEMPLATE = app
CONFIG -= app_bundle qt
CONFIG += console link_pkgconfig

PKGCONFIG += libpng zlib
INCLUDEPATH += common

SOURCES += \
    common/bitmap.cpp \
    common/os.cpp \
    common/util.cpp \
    xyz/main.cpp

HEADERS += \
    common/bitmap.h \
    common/os.h \
    common/util.h \
    common/file.h

win32:RC_ICONS += common/icon.ico

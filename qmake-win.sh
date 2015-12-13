#!/bin/bash -e
QMAKE="$HOME/Compilers/mxe/usr/i686-w64-mingw32.static/qt5/bin/qmake"
mkdir -p x86
for i in *.pro; do
  "$QMAKE" "$i"
  make
  mv release/"${i%.*}.exe" x86
  make clean
done

QMAKE="$HOME/Compilers/mxe/usr/x86_64-w64-mingw32.static/qt5/bin/qmake"
mkdir -p x86_64
for i in *.pro; do
  "$QMAKE" rpgconv.pro
  make
  mv release/"${i%.*}.exe" x86_64
  make clean
done

rm -rf debug release Makefile*
upx -qq x86/*.exe x86_64/*.exe

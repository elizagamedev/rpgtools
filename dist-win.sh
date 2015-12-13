#!/bin/bash -e
QMAKE="$HOME/Compilers/mxe/usr/i686-w64-mingw32.static/qt5/bin/qmake"
for arch in x86 x86_64; do
  cd "$arch"
  for exe in *.exe; do
    tool="${exe%.*}"
    mkdir -p "$tool"
    cp -f "$exe" ../LICENSE ../README.md "$tool"
    rm -f "$tool.zip"
    cd "$tool"
    zip -9 "../$tool.zip" *
    cd ..
  done
  cd ..
done


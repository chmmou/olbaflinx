#!/bin/bash

export SIGN=1
export SIGN_KEY=$1
QT_DIR=$2

cd ..
mkdir cbuild
cd cbuild

rm -rf * && cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$QT_DIR .. && make -j$(nproc) && make DESTDIR=AppDir install
~/Apps/AppImage/linuxdeploy --appdir AppDir --desktop-file=../res/olbaflinx.desktop --icon-file=../res/olbaflinx.png --output appimage --plugin qt
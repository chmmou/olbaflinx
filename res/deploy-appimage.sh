#!/bin/bash

export SIGN=1
export SIGN_KEY=$1

QT_DIR="$2"
if [ ! -z "$QT_DIR" ]
then
  USE_CMAKE_PREFIX_PATH="-DCMAKE_PREFIX_PATH=$QT_DIR"
  export QMAKE=$QT_DIR/bin/qmake
else
  USE_CMAKE_PREFIX_PATH=""
fi

USE_VERSION="$3"
if [ ! -z "$USE_VERSION" ]
then
  export VERSION=$USE_VERSION
else
  export VERSION=`git rev-parse --short HEAD`
fi

BUILD_DIR="build/appimage"

cd ..
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# example call: ./deploy-appimage.sh "sign key" "/path/to/qt" "version"

rm -rf * && cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release $USE_CMAKE_PREFIX_PATH ../.. && make -j$(nproc) && make DESTDIR=AppDir install
~/Apps/AppImage/linuxdeploy --appdir AppDir --desktop-file=../../res/olbaflinx.desktop --icon-file=../../res/olbaflinx.png --output appimage --plugin qt
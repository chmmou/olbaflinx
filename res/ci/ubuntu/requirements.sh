#!/bin/bash

#/**
# * Copyright (C) 2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
# *
# * This program is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program.  If not, see <https://www.gnu.org/licenses/>.
# */

set -euxo pipefail

# The same versions the workflow under .github/workflows pins. Picking the
# newest tag instead would drift between the image and the automated run, and
# libchipcard's newest tag is a beta.
GWENHYWFAR_VERSION="5.14.1"
AQBANKING_VERSION="6.9.2"
LIBCHIPCARD_VERSION="5.1.6"
ADS_VERSION="5.0.0"
QSQLCIPHER_VERSION="v6.6-1"

# Every ldconfig below is load bearing, not housekeeping. aqbanking generates
# its type sources with typemaker2, a program that ships with gwenhywfar and
# links against it. Without a refreshed cache the freshly installed library is
# invisible to it and the generation fails with "cannot open shared object
# file".
currentDirectory="$(dirname $(readlink -f ${BASH_SOURCE:-$0}))"

# Qt does not live in a system library directory in this image. libtool decides
# at configure time which directories count as system paths and silently drops
# a library handed to it as an absolute .so path outside of those. Without this
# libgwengui-qt6 ends up with no Qt entry in its DT_NEEDED at all, and every
# consumer then fails to link with undefined references into Qt Widgets.
QT_LIB_DIR="$(dirname "$(dirname "$(command -v qmake6 || command -v qmake)")")/lib"
printf '%s\n' "$QT_LIB_DIR" > /etc/ld.so.conf.d/qt6.conf
ldconfig

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/gwenhywfar
cd gwenhywfar
git checkout $GWENHYWFAR_VERSION
make -f Makefile.cvs
# The project links gwengui-qt6. gwenhywfar refuses qt5 and qt6 together.
./configure --prefix=/usr --with-guis="cpp qt6" LDFLAGS="-L$QT_LIB_DIR"
make --jobs=$(nproc) all
make install
ldconfig

objdump -p /usr/lib/libgwengui-qt6.so | grep -q "NEEDED.*libQt6Widgets" || {
    echo "libgwengui-qt6 was linked without Qt Widgets, libtool dropped them" >&2
    exit 1
}

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/aqbanking
cd aqbanking
git checkout $AQBANKING_VERSION
make -f Makefile.cvs
./configure --prefix=/usr
make typedefs
make typefiles
make --jobs=$(nproc) all
make install
ldconfig

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/libchipcard
cd libchipcard
git checkout $LIBCHIPCARD_VERSION
make -f Makefile.cvs
./configure --prefix=/usr
make --jobs=$(nproc) all
make install
ldconfig

cd $currentDirectory
git clone --recursive https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System.git
cd Qt-Advanced-Docking-System
git checkout $ADS_VERSION
mkdir cbuild
cd cbuild
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr ..
ninja
ninja install
ldconfig

cd $currentDirectory
git clone https://github.com/bAmpT/qsqlcipher-qt6-cmake.git
cd qsqlcipher-qt6-cmake
git checkout $QSQLCIPHER_VERSION
git submodule update --init --recursive
cp ../qsqlcipher.patch .
git apply ./qsqlcipher.patch
mkdir cbuild
cd cbuild 
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DQT_GENERATE_SBOM=OFF ..
ninja
ninja install
ldconfig

cd $currentDirectory
rm -rf libchipcard aqbanking gwenhywfar Qt-Advanced-Docking-System qsqlcipher-qt6-cmake

exit 0

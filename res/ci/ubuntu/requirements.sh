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
#
# The checkouts below use the commit, not the tag. A tag can be moved, so two
# runs of this script were not guaranteed to build the same sources. The version
# stays next to it because a commit alone does not say what is being upgraded
# when one of these is raised.
GWENHYWFAR_VERSION="5.14.1"
GWENHYWFAR_COMMIT="424949d0c6e61028eaccd6e46e4c0c41931c2b2f"
AQBANKING_VERSION="6.9.2"
AQBANKING_COMMIT="ffbcc03c3318ab4de01dcfa1ba75ca306364f5ea"
LIBCHIPCARD_VERSION="5.1.6"
LIBCHIPCARD_COMMIT="49062eb5ff54e8eddf4840969066178fe666c379"
ADS_VERSION="5.0.0"
ADS_COMMIT="433b0c90b44d8f17d059204176ff6e0d6d3783e3"
QSQLCIPHER_VERSION="v6.6-1"
QSQLCIPHER_COMMIT="18d2511bfb85e5f42f3599b83b3987e27b689da7"

# The Qt version this image carries. The Dockerfile passes it in; a direct run
# of this script has to say which Qt the sql driver is built against, because
# the plugin refuses to load under a Qt whose module version differs.
QT_VERSION="${QT_VERSION:?QT_VERSION has to be set, for example 6.8.3}"

# Every ldconfig below is load bearing, not housekeeping. aqbanking generates
# its type sources with typemaker2, a program that ships with gwenhywfar and
# links against it. Without a refreshed cache the freshly installed library is
# invisible to it and the generation fails with "cannot open shared object
# file".
currentDirectory="$(dirname $(readlink -f ${BASH_SOURCE:-$0}))"

# Qt does not live in a system library directory in this image.
QT_LIB_DIR="$(dirname "$(dirname "$(command -v qmake6 || command -v qmake)")")/lib"
printf '%s\n' "$QT_LIB_DIR" > /etc/ld.so.conf.d/qt6.conf
ldconfig

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/gwenhywfar
cd gwenhywfar
git checkout $GWENHYWFAR_COMMIT
make -f Makefile.cvs
# The project links gwengui-qt6. gwenhywfar refuses qt5 and qt6 together.
./configure --prefix=/usr --with-guis="cpp qt6"

# configure writes QT_LIBS as a list of absolute .so paths, and libtool
# silently discards every one of them whose directory it does not hold for a
# system path. The library then ends up with no Qt entry in its DT_NEEDED at
# all, and every consumer fails to link with undefined references into Qt
# Widgets. Handing it the same libraries as -L and -l makes libtool keep them.
qtLibs="$(grep -m1 '^QT_LIBS = ' gui/qt5/Makefile | sed 's/^QT_LIBS = //')"
qtLibs="-L$QT_LIB_DIR $(printf '%s' "$qtLibs" | sed -E 's#[^ ]*/lib([A-Za-z0-9_]+)\.so#-l\1#g')"

make --jobs=$(nproc) all QT_LIBS="$qtLibs"
# libtool relinks on install, so it needs the same value there.
make install QT_LIBS="$qtLibs"
ldconfig

objdump -p /usr/lib/libgwengui-qt6.so | grep -q "NEEDED.*libQt6Widgets" || {
    echo "libgwengui-qt6 was linked without Qt Widgets, libtool discarded them" >&2
    exit 1
}

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/aqbanking
cd aqbanking
git checkout $AQBANKING_COMMIT
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
git checkout $LIBCHIPCARD_COMMIT
make -f Makefile.cvs
./configure --prefix=/usr
make --jobs=$(nproc) all
make install
ldconfig

cd $currentDirectory
git clone --recursive https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System.git
cd Qt-Advanced-Docking-System
git checkout $ADS_COMMIT
mkdir cbuild
cd cbuild
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr ..
ninja
ninja install
ldconfig

cd $currentDirectory
git clone https://github.com/bAmpT/qsqlcipher-qt6-cmake.git
cd qsqlcipher-qt6-cmake
git checkout $QSQLCIPHER_COMMIT
git submodule update --init --recursive

# Two edits, both of which a patch file used to carry. The plugin has to answer
# to the driver name the project asks for, and its module version has to match
# the Qt it is built against. The version comes from QT_VERSION so that it is
# named once for this image; the patch pinned a second, different value, and the
# automated run pinned a third.
sed -i "s|^set(QT_REPO_MODULE_VERSION.*|set(QT_REPO_MODULE_VERSION \"${QT_VERSION}\")|" .cmake.conf
sed -i "s|OUTPUT_NAME qsqlitecipher|OUTPUT_NAME qsqlcipher|" qsqlcipher/CMakeLists.txt

mkdir cbuild
cd cbuild
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DQT_GENERATE_SBOM=OFF ..
ninja
ninja install
ldconfig

cd $currentDirectory
rm -rf libchipcard aqbanking gwenhywfar Qt-Advanced-Docking-System qsqlcipher-qt6-cmake

exit 0

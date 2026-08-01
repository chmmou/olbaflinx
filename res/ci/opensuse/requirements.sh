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

# The Qt version this image carries. openSUSE takes Qt from the package manager,
# so the value has to match what is installed there; the sql driver refuses to
# load under a Qt whose module version differs.
QT_VERSION="${QT_VERSION:?QT_VERSION has to be set, for example 6.9.1}"

# Every ldconfig below is load bearing, not housekeeping. aqbanking generates
# its type sources with typemaker2, a program that ships with gwenhywfar and
# links against it. Without a refreshed cache the freshly installed library is
# invisible to it and the generation fails with "cannot open shared object
# file". The ldconfig calls at the end stay, they cover the search paths.
currentDirectory="$(dirname $(readlink -f ${BASH_SOURCE:-$0}))"

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/gwenhywfar
cd gwenhywfar
git checkout $GWENHYWFAR_COMMIT
make -f Makefile.cvs
# The project links gwengui-qt6. gwenhywfar refuses qt5 and qt6 together. The
# release carries the qt6 binding, the thb-202505-qt6 branch is no longer
# needed for it.
./configure --prefix=/usr/local --with-guis="cpp qt6"
make --jobs=$(nproc) all
make install
ldconfig

# Qt comes from the package manager here and therefore sits in a system
# library directory, so libtool keeps it. The Ubuntu image installs Qt
# elsewhere and has to name the directory; see its requirements.sh. Checked
# all the same, because a silent drop here would only surface much later as
# undefined references into Qt Widgets.
objdump -p /usr/local/lib/libgwengui-qt6.so | grep -q "NEEDED.*libQt6Widgets" || {
    echo "libgwengui-qt6 was linked without Qt Widgets, libtool dropped them" >&2
    exit 1
}

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/aqbanking
cd aqbanking
git checkout $AQBANKING_COMMIT
ACLOCAL_FLAGS="-I /usr/local/share/aclocal -I /usr/share/aclocal $ACLOCAL_FLAGS" make -f Makefile.cvs
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure --prefix=/usr/local
LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH" make typedefs
LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH" make typefiles
LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH" make --jobs=$(nproc) all
LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH" make install

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/libchipcard
cd libchipcard
git checkout $LIBCHIPCARD_COMMIT
ACLOCAL_FLAGS="-I /usr/local/share/aclocal -I /usr/share/aclocal $ACLOCAL_FLAGS" make -f Makefile.cvs
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure --prefix=/usr/local
LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH" make --jobs=$(nproc) all
LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH" make install

cd $currentDirectory
git clone --recursive https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System.git
cd Qt-Advanced-Docking-System
git checkout $ADS_COMMIT
mkdir cbuild
cd cbuild 
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr/local ..
ninja
ninja install
ldconfig

cd $currentDirectory
git clone https://github.com/bAmpT/qsqlcipher-qt6-cmake.git
cd qsqlcipher-qt6-cmake
git checkout $QSQLCIPHER_COMMIT
git submodule update --init --recursive

# Two edits, both of which a patch file used to carry. See the Ubuntu script for
# why the version comes from QT_VERSION and not from a pinned value.
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

ldconfig /usr/local/lib/
ldconfig /usr/local/lib64/
ldconfig /usr/lib/
ldconfig /usr/lib64/
ldconfig /lib64/
ldconfig

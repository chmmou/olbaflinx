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

currentDirectory="$(dirname $(readlink -f ${BASH_SOURCE:-$0}))"

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/gwenhywfar
cd gwenhywfar
#git checkout $(git tag --sort=-creatordate | head -n 1)
git checkout thb-202505-qt6
make -f Makefile.cvs
./configure --prefix=/usr --with-guis="cpp qt5"
make --jobs=$(nproc) all
make install

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/aqbanking
cd aqbanking
git checkout $(git tag --sort=-creatordate | head -n 1)
make -f Makefile.cvs
./configure --prefix=/usr
make typedefs
make typefiles
make --jobs=$(nproc) all
make install

cd $currentDirectory
git clone --recursive https://git.aquamaniac.de/git/libchipcard
cd libchipcard
git checkout 5.1.6
make -f Makefile.cvs
./configure --prefix=/usr
make --jobs=$(nproc) all
make install

cd $currentDirectory
git clone --recursive https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System.git
cd Qt-Advanced-Docking-System
git checkout $(git tag --sort=-creatordate | head -n 1)
mkdir cbuild
cd cbuild 
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr ..
ninja
ninja install

cd $currentDirectory
git clone https://github.com/bAmpT/qsqlcipher-qt6-cmake.git
cd qsqlcipher-qt6-cmake
git checkout $(git tag --sort=-creatordate | head -n 1)
git submodule update --init --recursive
cp ../qsqlcipher.patch .
git apply ./qsqlcipher.patch
mkdir cbuild
cd cbuild 
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DQT_GENERATE_SBOM=OFF ..
ninja
ninja install

cd $currentDirectory
rm -rf libchipcard aqbanking gwenhywfar Qt-Advanced-Docking-System qsqlcipher-qt6-cmake

exit 0

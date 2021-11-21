## olbaflinx

[![Ubuntu (CTests)](https://github.com/chmmou/olbaflinx/actions/workflows/ubuntu-tests.yml/badge.svg?branch=develop)](https://github.com/chmmou/olbaflinx/actions/workflows/ubuntu-tests.yml)
[![experimental](http://badges.github.io/stability-badges/dist/experimental.svg)](https://github.com/chmmou/olbaflinx)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

OlbaFlinx is an multibank-capable online banking software for Linux based on the popular AqBanking library and the Qt 5 framework.

OlbaFlinx has the advantage that it is made for people who are just about to switch to Linux and or have switched and are looking for a simple financial software. The other advantage is that OlbaFlinx runs on any Linux Desktop Environment that supports the Qt 5 framework. This makes it possible for users to decide which desktop environment they want to use.

The idea to develop OlbaFlinx came from the fact that I was looking for a simple financial software for Linux that had the simplicity of [Banking4 (Windows / Mac)](https://subsembly.com/banking4.html). Unfortunately, none of the existing graphical financial software could convince me.

#### **_The development of OlbaFlinx is still in a very early state._**

## Features

tbd

## Build Introduction

### Dependencies

[CMake](https://cmake.org/download/) >= 3.16

    :~$ sudo apt install -y cmake 

[Qt 5](https://www.qt.io/download-qt-installer) latest release (5.15.2)

The Qt 5 from Ubuntu LTS 20.04 repository is too old. That's why we installing it from the Qt
Installer.

    :~$ cd ~
    :~$ wget -c "https://download.qt.io/official_releases/online_installers/qt-unified-linux-x64-online.run"    
    :~$ chmode +x qt-unified-linux-x64-online.run
    :~$ ./qt-unified-linux-x64-online.run

Or, if you preferred, from launchpad ppa. Or you can install from the launchpad
repository `ppa:beineri/opt-qt-5.15.2-focal` for Ubuntu LTS 20.04.

    :~$ sudo add-apt-repository ppa:beineri/opt-qt-5.15.2-focal
    :~$ sudo apt update
    :~$ # install all qt515* packages 

The qsqlcipher-qt5 dependency is used to store all sensitive data in encrypted form. This includes
account information, transactions, standing orders, documents, etc.

- [qsqlcipher-qt5](https://github.com/sjemens/qsqlcipher-qt5) >= 5.15

The dependencies `ktoblzcheck, aqbanking, gwenhywfar & libchipcard` are too old in the Ubuntu LTS
20.04 repository. That's why we download the tar archive and build it from scratch.

- [gwenhywfar](https://www.aquamaniac.de/rdm/projects/gwenhywfar/files) >= 5.7
- [aqbanking](https://www.aquamaniac.de/rdm/projects/aqbanking/files) >= 6.3
- [libchipcard](https://www.aquamaniac.de/rdm/projects/libchipcard/files) >= 5.1
- [ktoblzcheck](https://sourceforge.net/projects/ktoblzcheck/files/) >= 1.53

## Building from GIT

Some dependencies require additional dependencies. These must be installed first.

#### _gwenhywfar & aqbanking & libchipcard_

    :~$ sudo apt install -y libgcrypt20-dev libgcrypt20 libgpg-error-dev libgpg-error0 libgnutls28-dev libgnutls30 libgnutls-openssl27 libxmlsec1-gnutls libxmlsec1-dev libxmlsec1-gcrypt libxmlsec1-nss libxmlsec1-openssl libxmlsec1 libpcsclite1 libpcsclite-dev xmlsec1 doxygen 

#### _ktoblzcheck_

    :~$ sudo apt install -y libsqlite3-0 libsqlite3-dev sqlite3 python3-all python3-all-dev python3-xlrd 

#### _Install the `gwenhywfar` as first dependencies._

    :~$ wget -c "https://www.aquamaniac.de/rdm/attachments/download/390/gwenhywfar-5.7.3.tar.gz"
    :~$ tar -xzf gwenhywfar-5.7.3.tar.gz
    :~$ cd gwenhywfar-5.7.3
    :~$ ./configure --prefix=/usr/local --with-guis="qt5"
    :~$ make -j$(nproc) all
    :~$ make install
    :~$ cd ..
    :~$ rm -rf gwenhywfar-5.7.3.tar.gz gwenhywfar-5.7.3

#### _Install the `aqbanking` as second dependencies_

    :~$ wget -c "https://www.aquamaniac.de/rdm/attachments/download/386/aqbanking-6.3.2.tar.gz"
    :~$ tar -xzf aqbanking-6.3.2.tar.gz
    :~$ cd aqbanking-6.3.2
    :~$ ./configure --prefix=/usr/local
    :~$ make typedefs
    :~$ make types
    :~$ make -j$(nproc) all
    :~$ make install
    :~$ cd ..
    :~$ rm -rf aqbanking-6.3.2.tar.gz aqbanking-6.3.2

#### _Install the `libchipcard` as third dependencies_

    :~$ wget -c "https://www.aquamaniac.de/rdm/attachments/download/382/libchipcard-5.1.6.tar.gz"
    :~$ tar -xzf libchipcard-5.1.6.tar.gz
    :~$ cd libchipcard-5.1.6
    :~$ ./configure --prefix=/usr/local
    :~$ make -j$(nproc) all
    :~$ make install
    :~$ cd ..
    :~$ rm -rf libchipcard-5.1.6.tar.gz libchipcard-5.1.6

#### _At and finally install the `ktoblzcheck` dependencies_

    :~$ git clone https://git.code.sf.net/p/ktoblzcheck/code ktoblzcheck
    :~$ cd  ktoblzcheck
    :~$ git checkout 1.53
    :~$ mkdir cbuild
    :~$ cd cbuild
    :~$ cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_BANKDATA_DOWNLOAD=ON -DBUILD_STATIC=OFF ..
    :~$ make -j$(nproc) all
    :~$ make install
    :~$ cd ../..
    :~$ rm -rf ktoblzcheck

#### _Building OlbaFlinx from GIT_

TBD

## ToDo

- [ ] Get accounts, transactions, standing orders, electronic documents, ... (WiP)
- [ ] Create / send transactions, standing orders, accounts,...
- [ ] tbd ...
- [ ] Switch to Qt 6

> WiP = Work In Progress

## References

- [Qt Framework](https://www.qt.io/)
- [Qt SQL driver plugin for SQLCipher](https://github.com/sjemens/qsqlcipher-qt5)
- [AqBanking-Projektfamilie](https://www.aquamaniac.de/rdm/)
  - [gwenhywfar](https://www.aquamaniac.de/rdm/projects/gwenhywfar)
  - [aqbanking](https://www.aquamaniac.de/rdm/projects/aqbanking)
  - [libchipcard](https://www.aquamaniac.de/rdm/projects/libchipcard)
- [KtoBlzCheck](https://sourceforge.net/projects/ktoblzcheck)

## License

> OlbaFlinx Copyright (C) 2021, Alexander Saal <developer@olbaflinx.chm-projects.de>
>
> This program is free software: you can redistribute it and/or modify
> it under the terms of the GNU General Public License as published by
> the Free Software Foundation, either version 3 of the License, or
> (at your option) any later version.
>
> This program is distributed in the hope that it will be useful,
> but WITHOUT ANY WARRANTY; without even the implied warranty of
> MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
> GNU General Public License for more details.
>
> You should have received a copy of the GNU General Public License
> along with this program. If not, see <https://www.gnu.org/licenses/>.

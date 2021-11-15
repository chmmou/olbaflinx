## olbaflinx

OlbaFlinx is an online banking software for Linux based on the popular AqBaning library and the Qt 5 framework.

## Build Instructions
### Dependencies

- [CMake](https://cmake.org/download/) >= 3.16


    :~$ sudo apt install -y cmake 

- [Qt 5](https://www.qt.io/download-qt-installer) latest release (5.15.2)

The Qt 5 from Ubuntu LTS 20.04 repository is too old. That's why we installing it from  Qt Installer or from if you prefered from launchpad ppa.


    :~$: cd ~
    :~$: wget -c "https://download.qt.io/official_releases/online_installers/qt-unified-linux-x64-online.run"    
    :~$: chmode +x qt-unified-linux-x64-online.run
    :~$: ./qt-unified-linux-x64-online.run

Or you can install from the launchpad repository `ppa:beineri/opt-qt-5.15.2-focal` for Ubuntu LTS 20.04.


    sudo add-apt-repository ppa:beineri/opt-qt-5.15.2-focal
    sudo apt update
    sudo apt install -y qt515* 

The dependencies `ktoblzcheck, aqbanking, gwenhywfar & libchipcard` are too old in the ubuntu lts 20.04 repository.
That's why we download the tar archive and build it from scratch.

- [ktoblzcheck](https://sourceforge.net/projects/ktoblzcheck/files/) >= 1.53
- [gwenhywfar](https://www.aquamaniac.de/rdm/projects/gwenhywfar/files) >= 5.7
- [aqbanking](https://www.aquamaniac.de/rdm/projects/aqbanking/files) >= 6.3
- [libchipcard](https://www.aquamaniac.de/rdm/projects/libchipcard/files) >= 5.1 (optional)

### Building from GIT
tbd

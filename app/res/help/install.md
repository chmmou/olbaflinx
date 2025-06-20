[< zurück](index.md)

# Installation

## Einführung

### Anforderungen

- [CMake](https://github.com/Kitware/CMake/releases/tag/v3.22.3) >= 3.22


- [Qt 6](https://www.qt.io/download-qt-installer) >= 6.6.1
- [Qt SQLite Plugin für SQLCipher](https://github.com/bAmpT/qsqlcipher-qt6-cmake/tree/6.6-cmake) >= 6.6-cmake branch


- [gwenhywfar](https://www.aquamaniac.de/rdm/projects/gwenhywfar/files) >= 5.12.0
- [aqbanking](https://www.aquamaniac.de/rdm/projects/aqbanking/files) >= 6.6.0
- [libchipcard](https://www.aquamaniac.de/rdm/projects/libchipcard/files) >= 5.1.6 (für die Kartenleser-Unterstützung)

### Erstellungsprozess

Die Abhängigkeit qsqlcipher-qt6-cmake wird verwendet, um alle sensiblen Daten in verschlüsselter Form zu speichern.

Die Bibliotheken `aqbanking, gwenhywfar, libchipcard` sind im Pop!_OS 22.04 LTS Repository veraltet. Deshalb laden wir die jeweiligen TAR-Archive herunter und bauen es aus den Quellen selbst. Das hat auch den Vorteil, dass wir das `gwenhywfar gwengui-qt5` Plugin mit Qt 6 bauen können.

Einige Abhängigkeiten benötigen weitere Abhängigkeiten. Diese müssen zuerst installiert werden. Das Installieren der Abhängigkeiten hängt vom jeweiligen System ab was genutzt wird. Hier die Befehle für ein Ubuntu basiertes System.

Die `export PATH=...` Variable `$HOME/Qt/6.7.1/gcc_64` dient hier nur als Beispielinstallationspfad. Diese muss jedoch durch deinen Qt Installationspfad ersetzt werden. Das Setzen dieser `PATH` Variable ist nur temporär. Wird das Terminal / die Shell geschlossen ist dieser Wert der Variable wieder der normale Wert, der durch das System gesetzt wurde. 

##### _Abhängigkeiten für gwenhywfar & aqbanking & libchipcard_

```bash
sudo apt install -y libgcrypt20-dev libgcrypt20 libgpg-error-dev libgpg-error0 libgnutls28-dev libgnutls30 libgnutls-openssl27 libxmlsec1-gnutls libxmlsec1-dev libxmlsec1-gcrypt libxmlsec1-nss libxmlsec1-openssl libxmlsec1 libpcsclite1 libpcsclite-dev xmlsec1 doxygen 
sudo apt install -y libsqlite3-0 libsqlite3-dev sqlite3 python3-all python3-all-dev python3-xlrd
``` 

#### _Installation von `gwenhywfar` als erste Abhängigkeit._

```bash
cd /tmp
wget -c "https://www.aquamaniac.de/rdm/attachments/518/gwenhywfar-5.11.2beta.tar.gz"
tar -xzvf gwenhywfar-5.11.2beta.tar.gz
cd gwenhywfar-5.11.2beta
export PATH="$HOME/Qt/6.7.1/gcc_64/bin:$HOME/Qt/6.7.1/gcc_64/libexec:$PATH"
./configure --prefix=/usr/local --with-guis="qt5"
make -j$(nproc) all
sudo make install
rm -rf gwenhywfar-5.11.2beta.tar.gz gwenhywfar-5.11.2beta
```

#### _Installation von `aqbanking` als zweite Abhängigkeit_

```bash
cd /tmp
wget -c "https://www.aquamaniac.de/rdm/attachments/520/aqbanking-6.5.9beta.tar.gz"
tar -xzvf aqbanking-6.5.9beta.tar.gz
cd aqbanking-6.5.9beta
./configure --prefix=/usr/local
make typedefs
make types
make -j$(nproc) all
sudo make install
rm -rf aqbanking-6.5.9beta.tar.gz aqbanking-6.5.9beta
```

#### _Installation von `libchipcard` als dritte Abhängigkeit. Diese Abhängigkeit wird für die Verwendung von Kartenlesern (z.B.: für chipTAN USB) verwendet._

```bash
cd /tmp
wget -c "https://www.aquamaniac.de/rdm/attachments/382/libchipcard-5.1.6.tar.gz"
tar -xzvf libchipcard-5.1.6.tar.gz
cd libchipcard-5.1.6
./configure --prefix=/usr/local
make -j$(nproc) all
sudo make install
rm -rf libchipcard-5.1.6.tar.gz libchipcard-5.1.6
```

#### _Installation von qsqlcipher-qt6-cmake_

```bash
export PATH="$HOME/Qt/6.7.1/gcc_64/bin:$HOME/Qt/6.7.1/gcc_64/libexec:$PATH"
```

#### _Installation von OlbaFlinx aus dem GIT._

```bash
cd /tmp
git clone https://github.com/chmmou/olbaflinx.git
export PATH="$HOME/Qt/6.7.1/gcc_64/bin:$HOME/Qt/6.7.1/gcc_64/libexec:$PATH"
cd olbaflinx
# git checkout v1.0.0-alpha wenn verfügbar
mkdir cbuild && cd cbuild
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr/local ..
make -j$(nproc) all
sudo make install
```

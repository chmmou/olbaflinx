[< zurück](README.md)

# Installation

### Anforderungen

- [CMake](https://github.com/Kitware/CMake/releases/tag/v3.22.3) >= 3.22, dazu Ninja
---
- [Qt 6](https://www.qt.io/download-qt-installer) >= 6.8 LTS
- [Qt SQLite Plugin für SQLCipher](https://github.com/bAmpT/qsqlcipher-qt6-cmake/tree/6.6-cmake) >= 6.6-cmake branch
- [Qt Advanced Docking System](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System) >= 5.0.0
---
- [gwenhywfar](https://www.aquamaniac.de/rdm/projects/gwenhywfar/files) >= 5.14.1, gebaut mit `--with-guis="cpp qt6"`
- [aqbanking](https://www.aquamaniac.de/rdm/projects/aqbanking/files) >= 6.9.2 (für neue FinTS URLs und vor allem wegen behobene Fehler)
- [libchipcard](https://www.aquamaniac.de/rdm/projects/libchipcard/files) >= 5.1.6 (für Kartenleser-Unterstützung)
---

Ein C++20-fähiger Compiler wird vorausgesetzt. Die Konfiguration bricht ab, wenn ein installiertes Qt unterhalb von 6.8 gefunden wird.

### Erstellungsprozess

Die Abhängigkeit qsqlcipher-qt6-cmake wird verwendet, um alle sensiblen Daten in verschlüsselter Form zu speichern. Unter anderem wird auch die Abhängigkeit [Qt Advanced Docking System](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System) verwendet.

Die Bibliotheken `aqbanking, gwenhywfar, libchipcard` sind in den Distributions-Repositorien häufig veraltet. Deshalb laden wir die jeweiligen Projekte aus dem offiziellen Git Repository herunter und bauen sie aus den Quellen selbst. gwenhywfar wird dabei mit der Qt-6-Oberfläche gebaut, weil das Projekt gegen `gwengui-qt6` linkt.

Einige Abhängigkeiten benötigen weitere Abhängigkeiten. Diese müssen zuerst installiert werden. Das Installieren der Abhängigkeiten hängt vom jeweiligen System ab was genutzt wird. [Hier](res/ci/ubuntu/Dockerfile) die Befehle für ein Ubuntu basiertes System.

Zum einfachen Erstellen der Anwendung stelle ich ein [Dockerfile](res/ci/ubuntu/Dockerfile) für Ubuntu und eines für [openSUSE](res/ci/opensuse/Dockerfile) bereit. Die Versionen der Abhängigkeiten stehen in der jeweiligen [requirements.sh](res/ci/ubuntu/requirements.sh), jeweils mit dem Commit, auf den die Quelle festgenagelt ist. Ein Tag lässt sich verschieben, ein Commit nicht; zwei Läufe desselben Skripts bauen deshalb dieselben Quellen.

Die Qt-Version steht im Dockerfile an einer Stelle, als `ARG QT_VERSION`. Voreingestellt ist 6.8 LTS, die Mindestversion des Projekts. Für ein Abbild mit einer neueren Version:

```bash
docker build --build-arg QT_VERSION=6.11.1 -t olbaflinx-ci res/ci/ubuntu
```

Der automatische Lauf baut diese Abhängigkeiten nicht mehr selbst. Er zieht ein vorgebautes Abbild aus der GitHub Container Registry, das der Workflow [deps-image.yml](.github/workflows/deps-image.yml) aus demselben Dockerfile erzeugt. Das Abbild-Tag nennt die Qt-Version.

### Bauen

Die Build-Konfigurationen stehen in `CMakePresets.json`, gebaut wird nach `cbuild/`.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Der Preset `debug` baut die Tests mit, der Preset `release` nicht.

```bash
cmake --preset release
cmake --build --preset release
```

### Übersetzungen

Die Kataloge liegen in `res/i18n/`. Die `.qm`-Dateien entstehen im Bauverzeichnis und werden in die Anwendung eingebettet.

```bash
cmake --build --preset debug --target update_translations
cmake --build --preset debug --target release_translations
```

`update_translations` liest die Quellen erneut ein und schreibt neue Einträge in die `.ts`-Dateien.

### Installation

```bash
cmake --install cbuild/release --prefix /pfad/zum/ziel
```

Der Installationsschritt zieht über die Deployment-API von Qt die benötigten Qt-Bibliotheken, die Plugins und die Qt-Übersetzungen mit. Im Debug-Build landet die Installation unter `$HOME/.OlbaFlinx`.

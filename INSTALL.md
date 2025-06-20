[< zurück](README.md)

# Installation

### Anforderungen

- [CMake](https://github.com/Kitware/CMake/releases/tag/v3.22.3) >= 3.22
---
- [Qt 6](https://www.qt.io/download-qt-installer) >= 6.6.1
- [Qt SQLite Plugin für SQLCipher](https://github.com/bAmpT/qsqlcipher-qt6-cmake/tree/6.6-cmake) >= 6.6-cmake branch
- [Qt Advanced Docking System](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System) >= 4.3.0
---
- [gwenhywfar](https://www.aquamaniac.de/rdm/projects/gwenhywfar/files) >= 5.12.0 (für das Bauen des Qt5 Plugins mit Qt6)
- [aqbanking](https://www.aquamaniac.de/rdm/projects/aqbanking/files) >= 6.6.0 (für neue FinTS URLs und vor allem wegen behobene Fehler)
- [libchipcard](https://www.aquamaniac.de/rdm/projects/libchipcard/files) >= 5.1.6 (für Kartenleser-Unterstützung)
---

### Erstellungsprozess

Die Abhängigkeit qsqlcipher-qt6-cmake wird verwendet, um alle sensiblen Daten in verschlüsselter Form zu speichern. Unter anderem wird auch die Abhängigkeit [Qt Advanced Docking System](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System) verwendet. 

Die Bibliotheken `aqbanking, gwenhywfar, libchipcard` sind jedoch im Ubuntu 24.04 LTS Repository veraltet. Deshalb laden wir die jeweiligen Projekte aus dem offiziellen Git Repository herunter und bauen es aus den Quellen selbst. Das hat auch den Vorteil, dass wir das `gwenhywfar gwengui-qt5` Plugin mit Qt 6 bauen können.

Einige Abhängigkeiten benötigen weitere Abhängigkeiten. Diese müssen zuerst installiert werden. Das Installieren der Abhängigkeiten hängt vom jeweiligen System ab was genutzt wird. [Hier](res/ci/ubuntu/Dockerfile) die Befehle für ein Ubuntu basiertes System.

Zum einfachen Erstellen der Anwendung stelle ich ein [Dockerfile](res/ci/ubuntu/Dockerfile) bereit. Dieses Dockerfile wird auch auf GitHub verwendet, um die Tests laufen zu lassen.

Will man jedoch die Anwendung auf seinem eigenen Linux System erstellen empfehle ich, dass man sich die [Dockerfile](res/ci/ubuntu/Dockerfile)-Datei und die dazugehörige [requirements.sh](res/ci/ubuntu/requirements.sh)-Datei genauer anschaut.

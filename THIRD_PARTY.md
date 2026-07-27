# Drittanbieterkomponenten

Diese Datei erfasst alle Komponenten, gegen die OlbaFlinx gebaut oder gelinkt wird, mit Name, Version, Lizenz und Bezugsquelle. Sie ergänzt die Lizenztabelle in der [README](README.md) um Versionen, SPDX-Bezeichner und den Prüfstand.

Sie ist keine Rechtsberatung. Die SPDX-Bezeichner geben wieder, was die Lizenzdatei der jeweiligen Quelle sagt. Wo eine Quelle keinen eindeutigen Bezeichner hergibt, steht das in den Anmerkungen.

## Übersicht

| Komponente | Mindestversion im Build | Geprüfte Version | SPDX | Bezugsquelle |
|---|---|---|---|---|
| Qt 6 | 6.8 | 6.11.1 | `LGPL-3.0-only` | https://www.qt.io/ |
| aqbanking | 6.6 | 6.9.2 | `GPL-2.0-only OR GPL-3.0-only` | https://www.aquamaniac.de/rdm/projects/aqbanking |
| gwenhywfar | 5.12 | 5.14.1 | `LGPL-2.1-or-later` | https://www.aquamaniac.de/rdm/projects/gwenhywfar |
| gwengui-qt6 | 5.12 | 5.14.1 | `LGPL-2.1-or-later` | Teil von gwenhywfar |
| libchipcard | 5.1 | 5.1.6 | `LGPL-2.1-only` | https://www.aquamaniac.de/rdm/projects/libchipcard |
| qsqlcipher-qt6-cmake | keine | nicht feststellbar | `LGPL-3.0-only` | https://github.com/bAmpT/qsqlcipher-qt6-cmake |
| Qt Advanced Docking System | keine | 5.0.0 | `LGPL-2.1-or-later` | https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System |

Die Mindestversionen stehen in `CMakeLists.txt` und `src/CMakeLists.txt`. Die geprüften Versionen sind die, gegen die zuletzt gebaut wurde.

## Verwendete Qt-Module

`OlbaFlinxCore` linkt gegen Qt6::Core, Qt6::Sql, Qt6::Concurrent, Qt6::Network, Qt6::Xml, Qt6::Gui und Qt6::Widgets. `OlbaFlinxApp` linkt zusätzlich gegen Qt6::Help und Qt6::Svg. `LinguistTools` wird nur zur Bauzeit verwendet und nicht ausgeliefert.

Keines dieser Module steht ausschliesslich unter GPLv3. Die Liste der GPLv3-only-Module unter https://doc.qt.io/qt-6/licensing.html führt für Qt 6: Qt Canvas Painter, Qt CoAP, Qt Graphs, Qt GRPC, Qt HTTP Server, Qt Lottie Animation, Qt MQTT, Qt Network Authorization, Qt Qml Compiler, Qt Quick 3D, Qt Quick 3D Physics, Qt Quick Timeline, Qt Virtual Keyboard, Qt Wayland Compositor. Keines davon wird verwendet.

Qt Help ist gesondert geprüft, weil es zu den Qt-Werkzeugen gehört und diese laut Lizenzübersicht unter GPLv3 mit Qt-GPL-Ausnahme stehen. Die Modulseite https://doc.qt.io/qt-6/qthelp-index.html sagt für die Bibliothek etwas anderes: "Since Qt 5.4, these free software licenses are GNU Lesser General Public License, version 3, or the GNU General Public License, version 2." Qt Help ist damit kein GPLv3-only-Modul.

## Anmerkungen je Komponente

### Qt 6

Genutzt unter LGPLv3. Der Quellcode von Qt ist über https://download.qt.io/ und über https://code.qt.io/ beziehbar. An Qt wurden keine Änderungen vorgenommen.

`OlbaFlinxCore` wird als statische Bibliothek gebaut und linkt dynamisch gegen Qt. Die Entscheidung für das statische Linken der eigenen Kernbibliothek wurde am 27.07.2026 bewusst beibehalten. Grundlage ist, dass OlbaFlinx den vollständigen Quellcode öffentlich bereitstellt und Nutzer die Bibliothek damit austauschen und neu linken können.

### aqbanking

Die Datei `COPYING` des Quellbaums sagt wörtlich: "AqBanking is licensed under the GPLv2 or GPLv3 (see below)." Sie enthält anschliessend den vollständigen Text der GPL Version 2 und der GPL Version 3. Eine Formulierung "or any later version" steht nicht darin, die Quelldateien verweisen nur auf `COPYING`. Der Bezeichner lautet daher `GPL-2.0-only OR GPL-3.0-only` und nicht, wie sonst üblich, die `or-later`-Form.

Beiträge einzelner Autoren stehen zusätzlich unter der Modified BSD License. Das ist in `COPYING` aufgeführt und ändert die Lizenz des Gesamtwerks nicht.

### gwenhywfar

`COPYING` sagt: "Gwenhywfar is licensed under the GNU LGPL (see below) with this exception". Die Quelldateien tragen den üblichen Kopf mit "version 2.1 of the License, or (at your option) any later version". Daraus folgt `LGPL-2.1-or-later`.

Die Ausnahme lautet: Der Rechteinhaber erlaubt ausdrücklich das Übersetzen und Verbreiten von gwenhywfar zusammen mit dem OpenSSL Toolkit. Ein reiner SPDX-Bezeichner bildet diese Ausnahme nicht ab.

`src/os/portable_endian.h` ist von seinem Autor in die Public Domain gestellt.

`gwengui-qt6` ist Teil desselben Quellbaums und wird über eine eigene pkg-config-Datei eingebunden.

### libchipcard

`COPYING` sagt: "Libchipcard4 is licensed under the GNU LGPL v2.1 (see below)." Ohne "or any later version". Die Quelldateien verweisen nur auf `COPYING`. Der Bezeichner lautet daher `LGPL-2.1-only`.

Es gilt dieselbe OpenSSL-Ausnahme wie bei gwenhywfar.

### qsqlcipher-qt6-cmake

Der Treiber `libqsqlcipher.so` liegt unter `/usr/lib/qt6/plugins/sqldrivers/`. Er wurde ausserhalb der Paketverwaltung installiert und trägt keine Versionsinformation, die sich am installierten System ablesen liesse. Die README nennt den Stand `6.6-cmake`.

Der Treiber bindet SQLCipher ein. SQLCipher selbst steht unter einer BSD-artigen Lizenz von Zetetic LLC. Die eingebettete SQLCipher-Version ist am installierten Treiber nicht ablesbar.

Offen: Version und Lizenz sind gegen das Upstream-Repository und den tatsächlich verwendeten Bauzustand zu bestätigen.

### Qt Advanced Docking System

Installiert ist Version 5.0.0. Die README nannte bisher keine Version. Die Angabe 4.3.0 aus dem Refactoring-Entwurf trifft nicht zu.

Die Lizenzangabe `LGPL-2.1-or-later` stammt aus den Metadaten des installierten Distributionspakets. Das Upstream-Repository nennt LGPL-2.1. Die Unterscheidung zwischen `only` und `or-later` ist gegen die Datei `LICENSE` des Upstream-Repositorys zu bestätigen.

## Prüfstand

Geprüft am 27.07.2026 gegen:

- die Datei `COPYING` der lokalen Quellbäume von aqbanking, gwenhywfar und libchipcard,
- die Lizenzköpfe einzelner Quelldateien derselben Bäume,
- die Metadaten der installierten Distributionspakete für Qt 6 und das Qt Advanced Docking System,
- die Qt-Lizenzübersicht und die Modulseite von Qt Help.

Nicht geprüft:

- die Upstream-Repositories von qsqlcipher-qt6-cmake und dem Qt Advanced Docking System,
- die eingebettete SQLCipher-Version des installierten Treibers,
- ob die installierten Distributionspakete Änderungen gegenüber dem Upstream tragen.

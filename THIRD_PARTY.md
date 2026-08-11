# Drittanbieterkomponenten

Diese Datei erfasst alle Komponenten, gegen die OlbaFlinx gebaut oder gelinkt wird, mit Name, Version, Lizenz und Bezugsquelle. Sie ergänzt die Lizenztabelle in der [README](README.md) um Versionen, SPDX-Bezeichner und Bezugsquellen.

Sie ist keine Rechtsberatung. Die SPDX-Bezeichner geben wieder, was die Lizenzdatei der jeweiligen Quelle sagt. Wo eine Quelle keinen eindeutigen Bezeichner hergibt, steht das in den Anmerkungen.

## Übersicht

| Komponente | Mindestversion im Build | Verwendete Version | SPDX | Bezugsquelle |
|---|---|---|---|---|
| Qt 6 | 6.8 | 6.11.1 | `LGPL-3.0-only` | https://www.qt.io/ |
| aqbanking | 6.9.2 | 6.9.2 | `GPL-2.0-only OR GPL-3.0-only` | https://www.aquamaniac.de/rdm/projects/aqbanking |
| gwenhywfar | 5.14.1 | 5.14.1 | `LGPL-2.1-or-later` | https://www.aquamaniac.de/rdm/projects/gwenhywfar |
| gwengui-qt6 | 5.14.1 | 5.14.1 | `LGPL-2.1-or-later` | Teil von gwenhywfar |
| libchipcard | 5.1.6 | 5.1.6 | `LGPL-2.1-only` | https://www.aquamaniac.de/rdm/projects/libchipcard |
| qsqlcipher-qt6-cmake | keine | nicht feststellbar | `LGPL-3.0-only` | https://github.com/bAmpT/qsqlcipher-qt6-cmake |
| Qt Advanced Docking System | keine | 5.0.0 | `LGPL-2.1-or-later` | https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System |

Die Mindestversionen stehen in `CMakeLists.txt` und `src/CMakeLists.txt`. Die verwendeten Versionen sind die, gegen die zuletzt gebaut wurde.

## Anmerkungen je Komponente

### Qt 6

Genutzt unter LGPLv3. Der Quellcode von Qt ist über https://download.qt.io/ und über https://code.qt.io/ beziehbar. An Qt wurden keine Änderungen vorgenommen. Keines der eingebundenen Qt-Module steht ausschliesslich unter GPLv3.

`OlbaFlinxCore` wird als statische Bibliothek gebaut und linkt dynamisch gegen Qt. Die Entscheidung für das statische Linken der eigenen Kernbibliothek wurde bewusst beibehalten. Grundlage ist, dass OlbaFlinx den vollständigen Quellcode öffentlich bereitstellt und Nutzer die Bibliothek damit austauschen und neu linken können.

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

### Qt Advanced Docking System

Verwendet wird Version 5.0.0.

Die Lizenzangabe `LGPL-2.1-or-later` stammt aus den Metadaten des installierten Distributionspakets. Das Upstream-Repository nennt LGPL-2.1.

## Werkzeuge des Nachweises

Diese Komponenten stehen im CI-Abbild und treiben die Anwendung beim Lauf von
aussen. Sie werden weder gegen die Anwendung gelinkt noch mit ihr ausgeliefert,
und sie sind zum Bauen nicht nötig. Erfasst sind sie trotzdem, denn erfasst
wird jede Drittanbieterkomponente, nicht nur die des Bauens.

| Komponente | Version im Abbild | SPDX | Bezugsquelle |
|---|---|---|---|
| at-spi2-core | 2.60.4 | `LGPL-2.0-or-later` | https://gitlab.gnome.org/GNOME/at-spi2-core |
| Xvfb | 21.1.22 | `MIT` | https://www.x.org/ |
| matchbox-window-manager | 1.2.3 | `GPL-2.0-only` | https://www.yoctoproject.org/software-item/matchbox/ |
| dogtail | 1.0.7 | `GPL-2.0-or-later` | https://gitlab.com/dogtail/dogtail |
| glib2 (gsettings) | 2.88.0 | `LGPL-2.1-or-later` | https://gitlab.gnome.org/GNOME/glib |
| dconf | 0.49.0 | `LGPL-2.0-or-later` | https://gitlab.gnome.org/GNOME/dconf |

Die Bezeichner geben wieder, was die Datei `copyright` des jeweiligen
Distributionspakets nennt. Bei matchbox-window-manager steht dort "version 2
dated June, 1991" ohne die Formel "or any later version", daher `GPL-2.0-only`.
Bei Xvfb nennt die Datei die Fassung der X.Org Foundation, eine Abwandlung der
MIT-Lizenz.

Die beiden GPL-Komponenten berühren die Lizenz der Anwendung nicht. Sie werden
als eigenständige Programme aufgerufen, matchbox als Fensterverwalter und
dogtail aus einem Python-Prozess neben der Anwendung; kein Code von ihnen wird
gebunden.

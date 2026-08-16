# OlbaFlinx / OlbaFlinx Core

| Projekt         | Typ         | Lizenz                                                                              | Status                                                                                                                                                                                        |
|-----------------|-------------|-------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| OlbaFlinx Tests | Test        | [![Lizenz: GPL v3](res/license-gplv3.svg)](https://www.gnu.org/licenses/gpl-3.0)    | [![Ubuntu (CTests)](https://github.com/chmmou/olbaflinx/actions/workflows/ubuntu-tests.yml/badge.svg?branch=develop-qt6)](https://github.com/chmmou/olbaflinx/actions/workflows/ubuntu-tests.yml) |
| OlbaFlinx App   | Applikation | [![Lizenz: GPL v3](res/license-gplv3.svg)](https://www.gnu.org/licenses/gpl-3.0)    | [![experimental](res/experimental.svg)](https://github.com/chmmou/olbaflinx)                                                                                                                  |
| OlbaFlinx Core  | Bibliothek  | [![Lizenz: LGPL v3](res/license-lgplv3.svg)](https://www.gnu.org/licenses/lgpl-3.0) | [![experimental](res/experimental.svg)](https://github.com/chmmou/olbaflinx)                                                                                                                  |

### Beschreibung

OlbaFlinx ist eine multi bankfähige Online-Banking-Software für Linux, die auf der beliebten AqBanking-Bibliothek und dem Qt 6 Framework basiert.

OlbaFlinx hat den Vorteil, dass es für jeden gemacht ist, die gerade auf Linux umsteigen will oder umgestiegen ist und eine einfache Finanzsoftware sucht. Ein weiterer Vorteil ist, dass OlbaFlinx auf jeder Linux-Desktop-Umgebung läuft, die das Qt 6-Framework unterstützt. So können Benutzer selbst entscheiden, welche Desktop-Umgebung genutzt werden soll.

Die Idee zur Entwicklung von OlbaFlinx entstand aus der Tatsache, dass ich auf der Suche nach einer einfachen Finanzsoftware für Linux war, die die Einfachheit von [Banking4 (Windows / Mac)](https://subsembly.com/banking4.html) hat. Leider konnte mich keine der vorhandenen grafischen Finanzsoftware überzeugen.

OlbaFlinx Core beinhaltet die Businesslogik für OlbaFlinx.

**_Die Entwicklung von OlbaFlinx / OlbaFlinxCore befindet sich noch in einem sehr frühen Stadium._**

### Besonderheiten

1. Die Hauptbesonderheit ist das Sensible Daten wie, die Kontodaten, Transaktionen usw. verschlüsselt in einem Datenspeicher, genannt "Storage", abgelegt werden. Der Datenspeicher wird mit dem Passwort des Benutzers abgesichert. Das Passwort kann im Nachhinein abgeändert werden, sobald jedoch das Passwort verloren geht oder vergessen wird sind auch die Daten des Datenspeichers verloren. Das heißt, dass die Daten nicht wieder hergestellt werden können.
   1. Die Eingabe der PIN / TAN wird `nicht` gespeichert.
   2. Zum Verschlüsseln der Daten wird QSQLite mit SQLCipher verwendet.
   3. AqBanking legt die Kontodaten in Konfigurationsdateien ab, die im Hauptverzeichnis des Benutzers liegen. Diese Daten werden zwar im Datenspeicher ebenfalls abgelegt, sind dort aber verschlüsselt.
2. Eine Sicherung des Datenspeichers lässt sich über das Menü eines Eintrags in der Übersicht anlegen. Sie erfolgt auf Anforderung und nicht selbsttätig; darüber hinaus ist es ratsam, dass der Datenspeicher vom Benutzer selbst gesichert wird.
   1. Hier könnte eine externe Festplatte und oder ein externer USB-Stick helfen. Wenn es ein Cloud-Anbieter sein sollte, würde ich einen aus der EU, respektiv Deutschland, wählen.
   2. Ebenfalls ist es ratsam das Konfigurationsverzeichnis von AqBanking `~/.aqbanking` zu sichern.
   3. Zu empfehlen ist eine Backupstrategie für das Benutzerverzeichnis
3. Es werden keine expliziten benutzerbezogenen Daten; wie vollständiger Name, geb. Datum, Adresse usw., gespeichert, außer die angegebenen Kontodaten aus Punk 1.
   1. Die Kontodaten sind notwendig um mit der Bank zu kommunizieren.
4. Je nach Kontotyp kann auch ein Kartenleser verwendet werden. _Getestet habe ich es mit einem [cyberJack® RFID komfort (USB)](https://shop.reiner-sct.com/chipkartenleser-fuer-die-sicherheitsklasse-3/cyberjack-rfid-komfort-usb)._
5. Eine einfache zu bedienende Benutzeroberfläche
6. Die einfache Einrichtung der Zugangsdaten über die AqBanking Backend Dialoge
7. Strukturierte Verwaltung der Transaktionen / Dokumente / Daueraufträge
   1. Anzeige in Listen
   2. Sortierung der Spalten
   3. usw.
8. u. v. m.

### Aufbau

| Verzeichnis      | Inhalt                                                                       |
|------------------|------------------------------------------------------------------------------|
| `src/core/`      | `OlbaFlinxCore`, die Bibliothek mit der Businesslogik, ohne Oberflaeche       |
| `src/ui/`        | `OlbaFlinxUi`, die Widgets, Dialoge und Praesentationsmodelle                 |
| `src/main.cpp`   | `OlbaFlinxApp`, die ausfuehrbare Anwendung                                     |
| `tests/core/`    | Tests der Kernbibliothek, laufen ohne Anzeige                                 |
| `tests/ui/`      | Tests der Oberflaechenschicht                                                 |
| `res/`           | Ressourcen, Uebersetzungen unter `res/i18n/`, CI-Abbilder unter `res/ci/`     |

`OlbaFlinxCore` steht unter LGPLv3, `OlbaFlinxUi` und `OlbaFlinxApp` unter GPLv3. Beide Bibliotheken werden statisch gebunden.

### Bauen

Die Konfigurationen stehen in `CMakePresets.json`. Gebaut wird nach `cbuild/`.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Fuer die Auslieferung dient `--preset release`. Er baut ohne Tests und ohne Zusicherungen.

Voraussetzungen und die Herkunft der Abhaengigkeiten stehen in der [Installationsanleitung](INSTALL.md). Versionen, SPDX-Bezeichner und Bezugsquellen der Drittanbieterkomponenten stehen in [THIRD_PARTY.md](THIRD_PARTY.md).

## ToDo

- [ ] Konten einrichten / holen (WiP)
- [ ] Transaktionen (WiP)
- [ ] Daueraufträge (WiP)
- [ ] Elektronische Dokumente abholen
- [ ] Erstellen, speichern u. versenden (+verzögert) von (SEPA) Überweisungen / Daueraufträgen
- [x] ~~Verwendung von Qt 6~~
- [ ] Tests, Tests, Tests und ... noch mehr Tests :-)


- [ ] Multilingual 
  - [ ] de_DE (WiP)
  - [ ] en_US (WiP)
  - [ ] andere Übersetzungen
- [ ] Ausgeglichene und schöne Oberfläche


- [ ] Handbuch
- [ ] AppImage / Flatpack Pakete

> WiP = Work In Progress

## Referenzen

- [Qt 6 Framework](https://www.qt.io/)
- [Qt SQLite Plugin für SQLCipher](https://github.com/bAmpT/qsqlcipher-qt6-cmake/tree/6.6-cmake)
- [Qt Advanced Docking System](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System)
- [Aquamaniac-Projektfamilie](https://www.aquamaniac.de/rdm/)
   - [gwenhywfar](https://www.aquamaniac.de/rdm/projects/gwenhywfar)
   - [aqbanking](https://www.aquamaniac.de/rdm/projects/aqbanking)
   - [libchipcard](https://www.aquamaniac.de/rdm/projects/libchipcard)

### Maintainer

Das OlbaFlinx-Projekt wird derzeit von [Alexander Saal](https://github.com/chmmou/olbaflinx) entwickelt und betreut. Wenn du irgendwelche Fragen haben solltest, kannst du sie hier gerne stellen (nur Englisch und oder Deutsch).

### Lizenz

Der OlbaFlinx App Quellcode wird unter der [GNU General Public License Version 3](https://www.gnu.org/licenses/gpl-3.0) veröffentlicht. Der OlbaFlinx Core Quellcode wird unter der [GNU Lesser General Public License Version 3](https://www.gnu.org/licenses/lgpl-3.0) veröffentlicht. Weitere Informationen können aus der [Lizenz](LICENSE) Datei entnommen werden.

| Projekt                    | Lizenz                                                                                                        |
|----------------------------|---------------------------------------------------------------------------------------------------------------|
| aqbanking                  | [GPL v2 / GPL v3](https://www.aquamaniac.de/rdm/projects/aqbanking/repository/revisions/master/entry/COPYING) |
| gwenhywfar                 | [LGPL v2.1](https://www.aquamaniac.de/rdm/projects/gwenhywfar/repository/revisions/master/entry/COPYING)      |
| libchipcard                | [LGPL v2.1](https://www.aquamaniac.de/rdm/projects/libchipcard/repository/revisions/master/entry/COPYING)     |
| qsqlcipher-qt6-cmake       | [LGPL v3](https://github.com/bAmpT/qsqlcipher-qt6-cmake/blob/6.6-cmake/LICENSE)                               |
| Qt Advanced Docking System | [LGPL v2.1](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System?tab=LGPL-2.1-1-ov-file)            |
| Qt 6                       | [Mehrfach Lizenz](https://www.qt.io/qt-licensing)                                                             |
| Feather Icons              | [MIT](https://github.com/feathericons/feather/blob/main/LICENSE)                                              |

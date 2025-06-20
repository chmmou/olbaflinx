# OlbaFlinx / OlbaFlinx Core

| Projekt                    | Typ         | Lizenz                                                                                 | Status                                                                                                                                                                                        |
|----------------------------|-------------|----------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| OlbaFlinx App / Core Tests | Test        | [![Lizenz: GPL v3](images/license-gplv3.svg)](https://www.gnu.org/licenses/gpl-3.0)    | [![Ubuntu (CTests)](https://github.com/chmmou/olbaflinx/actions/workflows/ubuntu-tests.yml/badge.svg?branch=develop)](https://github.com/chmmou/olbaflinx/actions/workflows/ubuntu-tests.yml) |
| OlbaFlinx App              | Applikation | [![Lizenz: GPL v3](images/license-gplv3.svg)](https://www.gnu.org/licenses/gpl-3.0)    | [![experimental](images/experimental.svg)](https://github.com/chmmou/olbaflinx)                                                                                                               |
| OlbaFlinx Core             | Bibliothek  | [![Lizenz: LGPL v3](images/license-lgplv3.svg)](https://www.gnu.org/licenses/lgpl-3.0) | [![experimental](images/experimental.svg)](https://github.com/chmmou/olbaflinx)                                                                                                               |

OlbaFlinx ist eine multi bankfähige Online-Banking-Software für Linux, die auf der beliebten AqBanking-Bibliothek und dem Qt 6 Framework basiert.

OlbaFlinx hat den Vorteil, dass es für jeden gemacht ist, die gerade auf Linux umsteigen will oder umgestiegen ist und eine einfache Finanzsoftware sucht. Ein weiterer Vorteil ist, dass OlbaFlinx auf jeder Linux-Desktop-Umgebung läuft, die das Qt 6-Framework unterstützt. So können Benutzer selbst entscheiden, welche Desktop-Umgebung genutzt werden soll.

Die Idee zur Entwicklung von OlbaFlinx entstand aus der Tatsache, dass ich auf der Suche nach einer einfachen Finanzsoftware für Linux war, die die Einfachheit von [Banking4 (Windows / Mac)](https://subsembly.com/banking4.html) hat. Leider konnte mich keine der vorhandenen grafischen Finanzsoftware überzeugen.

OlbaFlinx Core beinhaltet die Basislogik von OlbaFlinx.

**_Die Entwicklung von OlbaFlinx / OlbaFlinxCore befindet sich noch in einem sehr frühen Stadium._**

### Besonderheiten

1. Die Hauptbesonderheit ist das sensible Datum wie, die Kontodaten, Transaktionen usw. verschlüsselt in einem sogenannten Datenspeicher, genannt "Storage", abgelegt werden. Der Datenspeicher wird mit dem Passwort des Benutzers abgesichert. Das Passwort kann im Nachhinein abgeändert werden, sobald jedoch das Passwort verloren geht oder vergessen wird sind auch die Daten des Datenspeichers verloren. Das heißt, dass die Daten nicht wieder hergestellt werden können.
    1. Die Eingabe der PIN / TAN wird `nicht` gespeichert.
    2. Zum Verschlüsseln der Daten wird QSQLite mit SQLCipher verwendet.
    3. AqBanking legt die Kontodaten in sogenannten Konfigurationsdateien ab, die im Hauptverzeichnis des Benutzers liegen. Diese Daten werden zwar im Datenspeicher ebenfalls abgelegt, sind dort aber verschlüsselt.
2. Eine weitere Besonderheit ist die, dass in regelmäßigen Abständen bei Nutzung der Anwendung eine Sicherung des Datenspeichers erfolgt. Es ist jedoch ratsam, dass der Datenspeicher selbst von dem Benutzer gesichert werden sollte.
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
8. u. v. m.

### Installation

Die Installationsanleitung kann [hier](install.md) gefunden werden.

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
- [Aquamaniac-Projektfamilie](https://www.aquamaniac.de/rdm/)
    - [gwenhywfar](https://www.aquamaniac.de/rdm/projects/gwenhywfar)
    - [aqbanking](https://www.aquamaniac.de/rdm/projects/aqbanking)
    - [libchipcard](https://www.aquamaniac.de/rdm/projects/libchipcard)

### Maintainer

Das OlbaFlinx-Projekt wird derzeit von [Alexander Saal](https://github.com/chmmou/olbaflinx) betreut. Wenn du irgendwelche Fragen haben solltest, kannst du sie hier gerne stellen (nur in Englisch oder deutsch).

### Lizenz

>> OlbaFlinx Application Copyright (c) 2021-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
>
>> OlbaFlinx Core Copyright (c) 2021-2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
>
> This program is free software: you can redistribute it and/or modify
> it under the terms of the GNU Lesser General Public License as published by
> the Free Software Foundation, either version 3 of the License, or
> (at your option) any later version.
>
> This program is distributed in the hope that it will be useful,
> but WITHOUT ANY WARRANTY; without even the implied warranty of
> MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
> GNU Lesser General Public License for more details.
>
> You should have received a copy of the GNU Lesser General Public License
> along with this program. If not, see <https://www.gnu.org/licenses/>.

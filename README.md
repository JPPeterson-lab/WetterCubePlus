# 🌦️ WetterCubePlus

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://wettercube.de/wp-content/themes/wettercube/assets/images/wettercube-plus-logo-hell.svg">
    <img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/wettercube-plus-logo.svg" width="520" alt="WetterCubePlus">
  </picture>
</p>

![Status](https://img.shields.io/badge/Status-Stable-brightgreen?style=flat-square)
![Version](https://img.shields.io/badge/Version-v2.1.8-blue?style=flat-square&cacheSeconds=1)
![Hardware](https://img.shields.io/badge/Hardware-ESP32--S3%20N16R8-red?style=flat-square&logo=espressif&logoColor=white)
![Display](https://img.shields.io/badge/Display-ILI9488%203.5%22%20480×320-informational?style=flat-square)
![Lizenz](https://img.shields.io/badge/Lizenz-Apache%202.0-blue?style=flat-square)
![Plattform](https://img.shields.io/badge/Platform-Arduino%20IDE-teal?style=flat-square&logo=arduino&logoColor=white)

*[English version below](#english)*

Ein kompaktes WLAN-Wetterdisplay auf Basis des **ESP32-S3 N16R8** mit 3,5"-Touchdisplay, DWD-Wetterwarnungen, Pollenflug und Web-Interface. Es ist die Weiterentwicklung des WetterCube. Die Wetterdaten kommen kostenlos von [Open-Meteo](https://open-meteo.com) und dem [Deutschen Wetterdienst (DWD)](https://www.dwd.de), ohne API-Key; Anzeige und Warnungen funktionieren derzeit nur auf Deutsch und in Deutschland.

> **🟢 Stabil:** Vollversion; weitere Updates folgen, die aktuelle Speerspitze der Familie ist der [WetterCube Fusion](https://github.com/JPPeterson-lab/WetterCube-Fusion).
>
> **🔓 Open Source (Apache 2.0):** Quellcode und Firmware liegen in diesem Repository. Texte, Bilder und 3D-Dateien: CC BY-NC 4.0, Bestandteile Dritter: [CREDITS.md](CREDITS.md), Name und Logos sind nicht freigegeben ([NOTICE](NOTICE)).
>
> **🔧 Hinweis zum Aufbau:** Beim Aufbau müssen Kabel gecrimpt und Bauteile gelötet werden. Wer das nicht möchte, ist mit dem [WetterCube Fusion](https://github.com/JPPeterson-lab/WetterCube-Fusion) besser bedient (komplettes Board mit Display).
>
> **🛠️ Softwareprojekt:** Das Gerät baust du selbst, fertige Geräte gibt es nicht. Kurzanleitung: [Bauanleitung](https://wettercube.de/bauanleitung/).
>
> **⚖️ Auf eigene Gefahr:** Keine Haftung, siehe die [Nutzungshinweise](https://wettercube.de/nutzungshinweise/). Nicht für sicherheitskritische Zwecke einsetzen; Verkabelung und Spannung sorgfältig prüfen. Fragen gern über das [Kontaktformular](https://wettercube.de/kontakt/) oder als [Issue](https://github.com/JPPeterson-lab/WetterCubePlus/issues).

## 📸 Bildschirme

<table>
  <tr>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-hauptscreen.png" width="300" alt="Hauptscreen"><br><sub>Hauptscreen</sub></td>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-pollen.png" width="300" alt="Pollenflug, 3 Tage"><br><sub>Pollenflug, 3 Tage</sub></td>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-warnkarte.png" width="300" alt="Regenradar mit Windpfeil"><br><sub>Regenradar mit Windpfeil</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-luftqualitaet.png" width="300" alt="Luftqualität"><br><sub>Luftqualität</sub></td>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-biowetter.png" width="300" alt="Biowetter"><br><sub>Biowetter</sub></td>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-gesundheit.png" width="300" alt="Gesundheitsübersicht"><br><sub>Gesundheitsübersicht</sub></td>
  </tr>
</table>

## ✨ Das kann der WetterCubePlus

- **Wetter:** Temperatur (Ist und gefühlt), Luftfeuchte, Luftdruck, Wind, UV-Index, Sonne und Mond, 4-Stunden-Vorhersage, 24-Stunden-Temperaturverlauf mit Regenbalken
- **Pollenflug** (eigene Allergene wählbar), **Luftqualität** und **DWD-Biowetter** mit Gesundheitsscreen für wetterfühlige Personen
- **DWD-Wetterwarnungen** mit Warnkarte und **Niederschlagsradar**
- **Hinweis-Kachel** für Regen, Sturm, Frost und UV, Nachtmodus, Dark Mode
- **Web-Oberfläche** unter `http://wettercubeplus.local`, WLAN-Einrichtung ohne App, Einstellungen sichern und einspielen
- **Updates (OTA)** per Display oder Browser, **WetterAmpel**-Schnittstelle, Taschenrechner und Bubblebreaker

## ⚡ Installieren

1. **[Web-Installer öffnen](https://jppeterson-lab.github.io/WetterCubePlus/)** (nur in Chrome oder Edge) und den ESP32-S3 per USB-C verbinden; das löscht den Flash-Speicher, auch gespeicherte WLAN-Daten und Einstellungen.
2. Beim ersten Start die **Touch-Kalibrierung** durchführen (4 Kreuze antippen), dann mit dem WLAN **„WetterCubePlus-Setup"** verbinden (Passwort steht auf dem Display); das Portal öffnet sich von selbst, sonst `192.168.4.1`.
3. WLAN-Zugangsdaten eingeben, den Standort **ohne Umlaute** (`Munchen` statt `München`) und die DWD-Region wählen, speichern. Danach ist das Gerät unter `http://wettercubeplus.local` erreichbar.

**Touch neu kalibrieren:** Beim Booten den Finger auf das Display halten, bis „Neukalibrierung" erscheint. Die manuelle Installation mit der Arduino IDE steht im [Wiki](https://wiki.wettercube.de/index.php/WetterCubePlus/Entwicklung).

## 🧊 Gehäuse zum Selbstdrucken

Das Gehäuse auf den Fotos (Unterteil und Deckel, Version 2, Format 3MF) gibt es kostenlos im Ordner [`gehaeuse/`](gehaeuse/). Empfehlung: PLA oder PETG, 0,4-mm-Düse. Ein fertiges Druckprofil auf Bambu MakerWorld folgt in Kürze. Lizenz der Dateien: CC BY-NC 4.0.

## 📖 Mehr erfahren

| Wo | Was |
|---|---|
| **[wettercube.de](https://wettercube.de/produkte/wettercubeplus/)** | Überblick, [Vergleich der Cubes](https://wettercube.de/vergleich/), [Roadmap](https://wettercube.de/roadmap/), [Bauanleitung](https://wettercube.de/bauanleitung/), [Hardware](https://wettercube.de/hardware/) |
| **[Wiki](https://wiki.wettercube.de/index.php/WetterCubePlus/Start)** | Hardware und Pinbelegung, Inbetriebnahme, Einstellungen, alle Screens, Updates, WetterAmpel, Fehlersuche, Entwicklung |
| **[Änderungsverlauf](CHANGELOG.md)** | was in welcher Version neu ist |

## 🙏 Credits und Lizenz

<p align="center">
  <a href="https://picopixel.io/de/"><picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://community.picopixel.io/branding/pico_dark_logo.png">
    <img src="https://community.picopixel.io/branding/pico_light_logo.png" alt="PicoPixel" width="220">
  </picture></a>
</p>

> 💙 **Danke an PicoPixel:** Die LVGL-UI des WetterCubePlus wird mit [PicoPixel](https://picopixel.io/de/) erstellt, einem kostenlosen, browserbasierten Drag-and-Drop-Editor für LVGL. Das ist kein Werbe- und kein Affiliate-Link, ich verdiene nichts daran.

Bibliotheken, Icons und Schriften von Dritten (LVGL, LovyanGFX, ArduinoJson, Meteocons, Montserrat und weitere) behalten ihre eigenen Lizenzen, Einzelheiten in [CREDITS.md](CREDITS.md). Wetterdaten: „Weather data by [Open-Meteo.com](https://open-meteo.com)“ ([CC BY 4.0](https://open-meteo.com/en/licence); die kostenlose Schnittstelle ist **nur für nicht-kommerzielle Nutzung**, wer kommerziell einsetzt, braucht dort eine eigene Lizenz), im WetterCube teils umgerechnet (zum Beispiel zu Pollenstufen). Pollen und Luftqualität: „Contains modified Copernicus Atmosphere Monitoring Service information“, über Open-Meteo. Ortssuche: „Location data by [GeoNames](https://www.geonames.org) via Open-Meteo“. Warnungen, Pollenflug, Biowetter und Radar: [DWD Opendata](https://opendata.dwd.de) (CC BY 4.0, „Quelle: Deutscher Wetterdienst“).

| Was | Lizenz | Datei |
|---|---|---|
| **Quellcode, Firmware** | Apache 2.0 | [LICENSE](LICENSE), [NOTICE](NOTICE) |
| **Texte, Bilder, Dokumentation, 3D-Druckdateien** | CC BY-NC 4.0 | [LICENSE-Dokumentation.txt](LICENSE-Dokumentation.txt) |
| **Bestandteile Dritter** | eigene Lizenzen (MIT, LGPL 2.1, Apache 2.0, SIL OFL, …) | [CREDITS.md](CREDITS.md) |

> **Versprechen:** Die private Nutzung von WetterCubePlus, Fusion und Next bleibt auch in Zukunft frei.

**Mitmachen:** Pull Requests sind willkommen und werden unter der Apache-Lizenz 2.0 aufgenommen (Abschnitt 5). Fehler und Wünsche gern als [Issue](https://github.com/JPPeterson-lab/WetterCubePlus/issues).

---

<a id="english"></a>

# 🌦️ WetterCubePlus (English)

A compact Wi-Fi weather display based on the **ESP32-S3 N16R8** with a 3.5" touchscreen, DWD weather warnings, pollen data and a web interface. It is the successor to the WetterCube. Weather data comes for free from [Open-Meteo](https://open-meteo.com) and the [German Weather Service (DWD)](https://www.dwd.de), no API key; the display language and the warnings currently only work in German and in Germany.

> **🟢 Stable:** full release; more updates will follow, the current spearhead of the family is the [WetterCube Fusion](https://github.com/JPPeterson-lab/WetterCube-Fusion).
>
> **🔓 Open source (Apache 2.0):** source code and firmware are in this repository. Texts, images and 3D files: CC BY-NC 4.0, third-party components: [CREDITS.md](CREDITS.md), name and logos are not licensed ([NOTICE](NOTICE)).
>
> **🔧 Note on assembly:** During assembly, cables have to be crimped and parts soldered. If you do not want that, the [WetterCube Fusion](https://github.com/JPPeterson-lab/WetterCube-Fusion) is the better choice (complete board with display).
>
> **🛠️ Software project:** You build the device yourself; there are no finished devices. Short guide: [build guide](https://wettercube.de/en/build-guide/).
>
> **⚖️ At your own risk:** No liability, see the [usage notes](https://wettercube.de/en/usage-notes/). Do not use for safety-critical purposes; check wiring and voltage carefully. Questions are welcome through the [contact form](https://wettercube.de/en/contact/) or as an [issue](https://github.com/JPPeterson-lab/WetterCubePlus/issues).

## 📸 Screens

<table>
  <tr>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-hauptscreen.png" width="300" alt="Main screen"><br><sub>Main screen</sub></td>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-pollen.png" width="300" alt="Pollen forecast, 3 days"><br><sub>Pollen forecast, 3 days</sub></td>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-warnkarte.png" width="300" alt="Rain radar with wind arrow"><br><sub>Rain radar with wind arrow</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-luftqualitaet.png" width="300" alt="Air quality"><br><sub>Air quality</sub></td>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-biowetter.png" width="300" alt="Biowetter"><br><sub>Biowetter</sub></td>
    <td align="center"><img src="https://wettercube.de/wp-content/themes/wettercube/assets/images/plus-gesundheit.png" width="300" alt="Health overview"><br><sub>Health overview</sub></td>
  </tr>
</table>

## ✨ What the WetterCubePlus can do

- **Weather:** temperature (actual and feels-like), humidity, air pressure, wind, UV index, sun and moon, 4-hour forecast, 24-hour temperature curve with rain bars
- **Pollen forecast** (choose your own allergens), **air quality** and **DWD Biowetter** with a health screen for weather-sensitive people
- **DWD weather warnings** with warning map and **precipitation radar**
- **Hint tile** for rain, storm, frost and UV, night mode, dark mode
- **Web interface** at `http://wettercubeplus.local`, Wi-Fi setup without an app, back up and restore settings
- **Updates (OTA)** from the display or the browser, **WetterAmpel** interface, calculator and Bubblebreaker

## ⚡ Installation

1. **[Open the web installer](https://jppeterson-lab.github.io/WetterCubePlus/)** (Chrome or Edge only) and connect the ESP32-S3 via USB-C; this erases the flash memory, including saved Wi-Fi data and settings.
2. At the first start do the **touch calibration** (tap 4 crosses), then connect to the Wi-Fi **"WetterCubePlus-Setup"**; the password is shown on the display; the portal opens by itself, otherwise `192.168.4.1`.
3. Enter your Wi-Fi credentials, the location **without umlauts** (`Munchen` instead of `München`) and the DWD region, save. Afterwards the device is reachable at `http://wettercubeplus.local`.

**Re-calibrate touch:** Hold a finger on the display while booting until "Neukalibrierung" appears. Manual installation with the Arduino IDE is described in the [wiki](https://wiki.wettercube.de/index.php/WetterCubePlus/Entwicklung).

## 🧊 Case to print yourself

The case in the photos (lower part and lid, version 2, 3MF format) is available for free in the folder [`gehaeuse/`](gehaeuse/). Recommendation: PLA or PETG, 0.4 mm nozzle. A ready-made print profile on Bambu MakerWorld will follow shortly. Licence of the files: CC BY-NC 4.0.

## 📖 Learn more

| Where | What |
|---|---|
| **[wettercube.de](https://wettercube.de/en/products/wettercubeplus/)** | overview, [comparison of the cubes](https://wettercube.de/en/comparison/), [roadmap](https://wettercube.de/en/roadmap/), [build guide](https://wettercube.de/en/build-guide/), [hardware](https://wettercube.de/en/hardware/) |
| **[Wiki](https://wiki.wettercube.de/index.php/WetterCubePlus/Start)** | hardware and pin assignment, getting started, settings, all screens, updates, WetterAmpel, troubleshooting, development |
| **[Changelog](CHANGELOG.en.md)** | what is new in which version |

## 🙏 Credits and licence

<p align="center">
  <a href="https://picopixel.io/"><picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://community.picopixel.io/branding/pico_dark_logo.png">
    <img src="https://community.picopixel.io/branding/pico_light_logo.png" alt="PicoPixel" width="220">
  </picture></a>
</p>

> 💙 **Thanks to PicoPixel:** The LVGL UI of the WetterCubePlus is created with [PicoPixel](https://picopixel.io/), a free, browser-based drag-and-drop editor for LVGL. This is not an advertisement or affiliate link, I earn nothing from it.

Libraries, icons and fonts by third parties (LVGL, LovyanGFX, ArduinoJson, Meteocons, Montserrat and more) keep their own licences, details in [CREDITS.md](CREDITS.md). Weather data: “Weather data by [Open-Meteo.com](https://open-meteo.com)” ([CC BY 4.0](https://open-meteo.com/en/licence); the free interface is for **non-commercial use only**, anyone using the software commercially needs their own licence from them), partly converted by the WetterCube (for example to pollen levels). Pollen and air quality: “Contains modified Copernicus Atmosphere Monitoring Service information”, via Open-Meteo. Location search: “Location data by [GeoNames](https://www.geonames.org) via Open-Meteo”. Warnings, pollen forecast, bio-weather and radar: [DWD Opendata](https://opendata.dwd.de) (CC BY 4.0, “Quelle: Deutscher Wetterdienst”).

| What | Licence | File |
|---|---|---|
| **Source code, firmware** | Apache 2.0 | [LICENSE](LICENSE), [NOTICE](NOTICE) |
| **Texts, images, documentation, 3D-print files** | CC BY-NC 4.0 | [LICENSE-Dokumentation.txt](LICENSE-Dokumentation.txt) |
| **Third-party components** | their own licences (MIT, LGPL 2.1, Apache 2.0, SIL OFL, …) | [CREDITS.md](CREDITS.md) |

> **Promise:** Private use of WetterCubePlus, Fusion and Next stays free in the future.

**Contributing:** Pull requests are welcome and are accepted under the Apache License 2.0 (section 5). Bugs and wishes as an [issue](https://github.com/JPPeterson-lab/WetterCubePlus/issues).

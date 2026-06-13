# WetterCubePlus

ESP32-S3 Wetterdisplay mit Touch, DWD-Warnungen, Pollenflug und Web-Interface.

> **⚠️ Beta-Version:** Dieses Projekt befindet sich noch in aktiver Entwicklung. Fehler und Breaking Changes sind möglich. / This project is still in active development. Bugs and breaking changes may occur.

**[→ Web-Installer](https://jppeterson-lab.github.io/WetterCubePlus/)**

## Hardware

| Komponente | Typ |
|---|---|
| MCU | ESP32-S3 N16R8 (16 MB Flash, 8 MB PSRAM) |
| Display | ILI9488 3,5" 480×320 SPI |
| Touch | XPT2046 resistiv |
| Backlight | PWM über GPIO 17 |

### Pin-Belegung

| Funktion | GPIO |
|---|---|
| TFT SCLK | 14 |
| TFT MOSI | 13 |
| TFT MISO | 12 |
| TFT DC | 2 |
| TFT CS | 15 |
| TFT RST | 16 |
| TFT BL (PWM) | 17 |
| Touch CS | 21 |
| Touch IRQ | 18 |
| Touch CLK | 6 |
| Touch MOSI | 7 |
| Touch MISO | 8 |

> **Achtung:** GPIO 33–37 sind intern für das OPI-PSRAM reserviert – nicht verwenden.

## Features

- **Wetterdaten** – Temperatur, Luftfeuchtigkeit, Windgeschwindigkeit/-richtung, Luftdruck, UV-Index, Sonnenauf-/-untergang (Open-Meteo)
- **4-Stunden-Vorhersage** – Wetter-Icons, Temperatur, Wind
- **Pollenflug** – 8 Allergene (DWD + Open-Meteo Air Quality), Warnschwelle einstellbar
- **Regenwarnung** – aktuelle Stunde + nächste Stunde
- **DWD Wetterwarnungen** – bis zu 5 aktive Warnungen (Level ≥ 2)
- **DWD Warnkarte** – Deutschland-Warnkarte als eigener Screen
- **WebUI** – Einstellungen über `wettercubeplus.local` (Helligkeit, Dimm-Timeout, Warnungen, Region)
- **Captive Portal** – WLAN-Ersteinrichtung ohne App
- **OTA-Update** – Firmware-Update über WebUI direkt von GitHub Releases
- **Touch-Kalibrierung** – automatisch beim Erststart, Neustart per Dauerdruck beim Booten

## Screens

1. **Hauptscreen** – Wetter, Uhrzeit, Pollen-Schnellinfo
2. **Wettervorhersage** – 4×3-Stunden-Slots mit Icon, Temperatur, Wind
3. **Pollenvorhersage** – alle Allergene, DWD-Stufen (heute/morgen/übermorgen)
4. **DWD Warnkarte** – Deutschland-Karte mit aktuellen Warngebieten
5. **Regenwarnung** / **Pollenwarnung** – blinkendes Warn-Screen bei Überschreitung

## Installation

### Web-Installer (empfohlen)

1. [Web-Installer öffnen](https://jppeterson-lab.github.io/WetterCubePlus/)
2. ESP32-S3 per USB-C verbinden (Chrome oder Edge)
3. „Installieren" klicken und Anweisungen folgen

### Manuelle Installation (Arduino IDE)

**Benötigte Bibliotheken:**
- LovyanGFX ≥ 1.2.x
- LVGL 8.3.x (lv_conf.h aus diesem Repo in Arduino libraries kopieren)
- ArduinoJson ≥ 6.x

**Kompiliereinstellungen (Arduino IDE):**
- Board: ESP32S3 Dev Module
- PSRAM: OPI PSRAM
- Flash Size: 16 MB
- Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)

## Erstkonfiguration

1. Gerät startet und öffnet WLAN-Hotspot **„WetterCubePlus-Setup"**
2. Verbinden → Browser öffnet Einrichtungsportal automatisch (alternativ: `192.168.4.1`)
3. WLAN-Zugangsdaten eingeben
4. Standort eingeben (ohne Umlaute: `Munchen`, `Koeln`)
5. DWD-Pollenregion wählen
6. Speichern → Gerät startet neu und verbindet sich

Nach dem Neustart erreichbar unter **`wettercubeplus.local`** oder der angezeigten IP-Adresse.

### Touch-Kalibrierung

Beim Erststart wird die Touch-Kalibrierung automatisch durchgeführt (4 Punkte antippen).  
Neustart der Kalibrierung: beim Booten Finger auf das Display halten bis „Neukalibrierung" erscheint.

## Datenquellen

| Quelle | Daten |
|---|---|
| [Open-Meteo](https://open-meteo.com) | Wetter, Luftqualität/Pollen – kein API-Key nötig |
| [DWD Opendata](https://opendata.dwd.de) | Pollenflug-Vorhersage, Wetterwarnungen |
| [DWD Warnkarte](https://www.dwd.de) | Deutschland-Warnkarte PNG |

## Lizenz

MIT License – siehe [LICENSE](LICENSE)

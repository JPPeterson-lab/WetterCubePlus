# 🌦️ WetterCubePlus

Ein kompaktes WLAN-Wetterdisplay auf Basis des **ESP32-S3 N16R8** mit 3,5"-Touchdisplay, DWD-Wetterwarnungen, Pollenflug und Web-Interface.
Dieses Projekt ist die Weiterentwicklung des [WetterCube](https://github.com/JPPeterson-lab/wettercube) und mit KI-Hilfe programmiert.

Wetterdaten kommen kostenlos von [Open-Meteo](https://open-meteo.com) und dem [Deutschen Wetterdienst (DWD)](https://www.dwd.de) – kein API-Key nötig.

> **⚠️ Beta-Version:** Dieses Projekt befindet sich noch in aktiver Entwicklung. Fehler und Breaking Changes sind möglich. / This project is still in active development. Bugs and breaking changes may occur.

---

## ✨ Funktionen

| | Funktion | Details |
|---|---|---|
| 🌡️ | **Wetterdaten** | Temperatur (Ist & gefühlt), Luftfeuchtigkeit, Luftdruck, UV-Index, Sonnenauf-/-untergang |
| 💨 | **Wind** | Geschwindigkeit & Richtung |
| 🕐 | **4-Stunden-Vorhersage** | Icons, Temperatur, Wind pro Slot |
| 🌿 | **Pollenflug** | Top-3 Allergene stündlich (Open-Meteo), 3-Tage-Forecast für 8 Allergene (DWD) |
| 🌧️ | **Regenwarnung** | Blinkendes Warn-Screen 60 Min. vor erwartetem Regen |
| ⚠️ | **DWD Wetterwarnungen** | Bis zu 5 aktive Warnungen als farbige Karten (DWD-Warnstufe), tippbar für Details; blinkender Indikator auf dem Hauptscreen |
| 🗺️ | **Niederschlagsradar** | Bundesland-Radarkarte (DWD WMS), alle 10 Min. aktualisiert, mit Standort-Marker |
| ☀️ | **Sonne & Mond** | UV-Index (Tages-Max), Sonnenaufgang & Sonnenuntergang |
| 🖥️ | **WebUI** | Alle Einstellungen unter `wettercubeplus.local` – kein Flashen nötig |
| 📡 | **Captive Portal** | WLAN-Ersteinrichtung ohne App |
| 🔄 | **OTA-Update** | Firmware-Update per WebUI über WLAN |
| 👆 | **Touch-Kalibrierung** | Automatisch beim Erststart, jederzeit neu auslösbar |

---

## 🎬 Screens

| # | Screen | Inhalt |
|---|---|---|
| 1 | **Hauptscreen** | Wetter aktuell, Uhrzeit, Datum, Top-3 Pollen (nächste Stunde) |
| 2 | **Wettervorhersage** | 4×3-Stunden-Slots mit Icon, Temperatur, Wind |
| 3 | **Pollenvorhersage (Stunden)** | Pollenbelastung der nächsten 3 Stunden (Open-Meteo, stündlich) |
| 4 | **Pollenvorhersage (Tage)** | 8 Allergene (DWD), Stufen heute/morgen/übermorgen |
| 5 | **Niederschlagsradar** | Bundesland-Ausschnitt, DWD WMS, 10-Min.-Aktualisierung |
| 6 | **Sonne & Mond** | UV-Index (Tages-Max), Sonnenaufgang & Sonnenuntergang |
| 7 | **Warnscreen** | Blinkendes Warn-Screen bei Regen- oder Pollenwarnung (Antippen für Details) |
| 8 | **DWD Warnkarte** | Standortbezogene DWD-Warnmeldungen als farbige Karten, nach Warnstufe sortiert |

---

## ⚡ Web-Installer (kein Arduino IDE nötig)

**👉 [WetterCubePlus Web-Installer öffnen](https://jppeterson-lab.github.io/WetterCubePlus/)**

> Funktioniert nur in **Google Chrome** oder **Microsoft Edge** (WebSerial API).
> ESP32-S3 per **USB-C** verbinden, dann den Anweisungen folgen.

---

## 🔧 Erstkonfiguration

1. Gerät startet – **Touch-Kalibrierung**: 4 Kreuzmarkierungen nacheinander antippen
2. WLAN-Hotspot **„WetterCubePlus-Setup"** verbinden (Smartphone oder PC)
3. Browser öffnet Portal automatisch – alternativ `192.168.4.1`
4. WLAN-Zugangsdaten eingeben
5. Standort eingeben – **ohne Umlaute**: `Munchen statt München`, `Koln statt Köln`, `Dusseldorf statt Düsseldorf`
6. DWD-Pollenregion / Bundesland wählen (gilt auch für die Radarkarte)
7. Speichern & Neustart

Nach dem Neustart erreichbar unter **`http://wettercubeplus.local`** oder der auf dem Display angezeigten IP.

### 🖱️ Touch neu kalibrieren

Beim Booten Finger auf das Display halten bis „Neukalibrierung" erscheint, dann loslassen.

---

## ⚙️ Web-Konfiguration

| Bereich | Einstellung |
|---|---|
| **Display** | Helligkeit: 10–100 % (Slider, sofort aktiv) |
| | Dimm-Timeout: Aus / 1 / 3 / 5 / 10 Minuten |
| | Nachtmodus: Von/Bis (15-Min-Schritte), eigene Nacht-Helligkeit 0–50 % |
| **Warnungen** | Regenwarnung ein/aus |
| | Pollenwarnung ein/aus |
| | Pollen-Schwellwert: Gering / Mittel / Hoch / Sehr hoch |
| **DWD-Region** | Bundesland-Auswahl für Wetterwarnungen & Radarkarte |
| **Firmware** | OTA-Update: neue Version per WLAN – kein USB nötig |

---

## 🌿 Pollen-Architektur

| Screen | Quelle | Aktualisierung |
|---|---|---|
| Hauptscreen Top-3 | Open-Meteo Stundenwerte (nächste Stunde) | stündlich |
| Pollenwarnung | Open-Meteo Stundenwerte (nächste Stunde) | stündlich |
| 3-Tage-Pollenforecast | DWD Pollenflug-Gefahrenindex (8 Allergene) | stündlich |

---

## 📡 Datenquellen

| Quelle | Daten | Intervall |
|---|---|---|
| [Open-Meteo](https://open-meteo.com) | Wetter, Pollen stündlich | 60 Min. |
| [DWD Opendata](https://opendata.dwd.de) | Pollenflug-Tagesvorhersage, Wetterwarnungen | 60 Min. |
| [DWD WMS](https://maps.dwd.de) | Niederschlagsradar (bundeslandbezogen) | 10 Min. |

---

## 🔌 Hardware & Pinbelegung

| Komponente | Typ |
|---|---|
| MCU | ESP32-S3 N16R8 (16 MB Flash, 8 MB OPI-PSRAM) |
| Display | ILI9488 3,5" · 480×320 px · SPI |
| Touch | XPT2046 resistiv · eigener SPI-Bus |
| Backlight | PWM über ESP-IDF LEDC · GPIO 17 |

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

> ⚠️ **GPIO 33–37** sind intern für das OPI-PSRAM reserviert – **nicht verwenden!**

---

## 🛠️ Manuelle Installation (Arduino IDE)

**Benötigte Bibliotheken:**

| Bibliothek | Version |
|---|---|
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | ≥ 1.2.x |
| [LVGL](https://lvgl.io) | 8.3.x |
| [ArduinoJson](https://arduinojson.org) | ≥ 6.x |
| [lodepng](https://lodev.org/lodepng/) | im LVGL-Paket enthalten |

**Arduino IDE Board-Einstellungen:**

| Einstellung | Wert |
|---|---|
| Board | ESP32S3 Dev Module |
| PSRAM | OPI PSRAM |
| Flash Size | 16 MB |
| Partition Scheme | **Custom** (partitions.csv im Sketch-Ordner) |

---

## 🙏 Credits

| Ressource | Urheber | Lizenz |
|---|---|---|
| Wetter-Icons | [Dovora Weather Icons](https://www.dovora.com/resources/weather-icons/) | [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) |
| Wetterdaten | [Open-Meteo](https://open-meteo.com) | Open-Meteo API (kostenlos) |
| DWD-Daten | [Deutscher Wetterdienst](https://opendata.dwd.de) | DWD Opendata |
| DWD WMS | [DWD GeoServer](https://maps.dwd.de) | DWD Opendata |
| GUI-Framework | [LVGL](https://lvgl.io) | MIT |

Weitere Details in [CREDITS.md](CREDITS.md).

## 📄 Lizenz

MIT License – siehe [LICENSE](LICENSE)

Die enthaltenen Wetter-Icons unterliegen der **CC BY-SA 4.0**-Lizenz (siehe [CREDITS.md](CREDITS.md)).

---
---

# 🌦️ WetterCubePlus (English)

A compact Wi-Fi weather display based on the **ESP32-S3 N16R8** with a 3.5" touchscreen, DWD weather warnings, pollen data, and a web interface.
This project is the successor to [WetterCube](https://github.com/JPPeterson-lab/wettercube), built and programmed with AI assistance.

Weather data is provided free of charge by [Open-Meteo](https://open-meteo.com) and the [German Weather Service (DWD)](https://www.dwd.de) – no API key required.

---

## ✨ Features

| | Feature | Details |
|---|---|---|
| 🌡️ | **Weather data** | Temperature (actual & feels-like), humidity, air pressure, UV index, sunrise/sunset |
| 💨 | **Wind** | Speed & direction |
| 🕐 | **4-hour forecast** | Icons, temperature, and wind per time slot |
| 🌿 | **Pollen** | Top-3 allergens hourly (Open-Meteo), 3-day forecast for 8 allergens (DWD) |
| 🌧️ | **Rain warning** | Flashing warning screen 60 min before expected rainfall |
| ⚠️ | **DWD weather warnings** | Up to 5 active warnings as color-coded cards (DWD severity level), tap for details; blinking indicator on main screen |
| 🗺️ | **Precipitation radar** | Federal-state radar map (DWD WMS), updated every 10 min, with location marker |
| ☀️ | **Sun & Moon** | UV index (daily max), sunrise & sunset times |
| 🖥️ | **WebUI** | All settings at `wettercubeplus.local` – no reflashing needed |
| 📡 | **Captive Portal** | Wi-Fi setup without an app |
| 🔄 | **OTA update** | Firmware update via WebUI over Wi-Fi |
| 👆 | **Touch calibration** | Automatic on first boot, re-triggerable anytime |

---

## 🎬 Screens

| # | Screen | Content |
|---|---|---|
| 1 | **Main screen** | Current weather, time, date, top-3 pollen (next hour) |
| 2 | **Forecast** | 4×3-hour slots with icon, temperature, wind |
| 3 | **Pollen forecast (hourly)** | Pollen levels for the next 3 hours (Open-Meteo, updated hourly) |
| 4 | **Pollen forecast (daily)** | 8 allergens (DWD), levels today/tomorrow/day after |
| 5 | **Precipitation radar** | Federal-state map view, DWD WMS, updated every 10 min |
| 6 | **Sun & Moon** | UV index (daily max), sunrise & sunset times |
| 7 | **Warning screen** | Flashing alert for rain or pollen warnings (tap for details) |
| 8 | **DWD warning list** | Location-based DWD warnings as color-coded cards by severity level |

---

## ⚡ Web Installer (No Arduino IDE Required)

**👉 [Open WetterCubePlus Web Installer](https://jppeterson-lab.github.io/WetterCubePlus/)**

> Works only in **Google Chrome** or **Microsoft Edge** (WebSerial API).
> Connect the ESP32-S3 via **USB-C** and follow the on-screen instructions.

---

## 🔧 Initial Setup

1. Device boots – **Touch calibration**: tap the 4 cross markers in sequence
2. Connect to Wi-Fi hotspot **"WetterCubePlus-Setup"** (phone or PC)
3. The setup portal opens automatically – or open `192.168.4.1` in your browser
4. Enter your Wi-Fi credentials
5. Enter your location – **no umlauts**: `Munchen`, `Koeln`, `Dusseldorf`
6. Select your DWD region / federal state (also used for the radar map)
7. Save & restart

After restarting, access the device at **`http://wettercubeplus.local`** or the IP address shown on the display.

### 🖱️ Re-calibrate touch

Hold your finger on the display during boot until "Neukalibrierung" appears, then release.

---

## ⚙️ Web Configuration

| Category | Setting |
|---|---|
| **Display** | Brightness: 10–100 % (slider, applied instantly) |
| | Dimming timeout: Off / 1 / 3 / 5 / 10 minutes |
| | Night mode: From/To (15-min steps), separate night brightness 0–50 % |
| **Warnings** | Rain warning on/off |
| | Pollen warning on/off |
| | Pollen threshold: Low / Moderate / High / Very High |
| **DWD region** | Select federal state for weather warnings & radar map |
| **Firmware** | OTA update: install new firmware over Wi-Fi – no USB needed |

---

## 🌿 Pollen Architecture

| Screen | Source | Update interval |
|---|---|---|
| Main screen top-3 | Open-Meteo hourly (next hour) | Every 60 min |
| Pollen warning | Open-Meteo hourly (next hour) | Every 60 min |
| 3-day pollen forecast | DWD pollen index (8 allergens) | Every 60 min |

---

## 📡 Data Sources

| Source | Data | Interval |
|---|---|---|
| [Open-Meteo](https://open-meteo.com) | Weather, hourly pollen | 60 min |
| [DWD Opendata](https://opendata.dwd.de) | Daily pollen forecast, weather warnings | 60 min |
| [DWD WMS](https://maps.dwd.de) | Precipitation radar (federal state) | 10 min |

---

## 🔌 Hardware & Pin Assignment

| Component | Type |
|---|---|
| MCU | ESP32-S3 N16R8 (16 MB Flash, 8 MB OPI-PSRAM) |
| Display | ILI9488 3.5" · 480×320 px · SPI |
| Touch | XPT2046 resistive · dedicated SPI bus |
| Backlight | PWM via ESP-IDF LEDC · GPIO 17 |

| Function | GPIO |
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

> ⚠️ **GPIO 33–37** are reserved internally for OPI-PSRAM – **do not use!**

---

## 🛠️ Manual Installation (Arduino IDE)

**Required libraries:**

| Library | Version |
|---|---|
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | ≥ 1.2.x |
| [LVGL](https://lvgl.io) | 8.3.x |
| [ArduinoJson](https://arduinojson.org) | ≥ 6.x |
| [lodepng](https://lodev.org/lodepng/) | included in LVGL package |

**Arduino IDE board settings:**

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| PSRAM | OPI PSRAM |
| Flash Size | 16 MB |
| Partition Scheme | **Custom** (partitions.csv im Sketch-Ordner) |

---

## 🙏 Credits

| Resource | Author | License |
|---|---|---|
| Weather Icons | [Dovora Weather Icons](https://www.dovora.com/resources/weather-icons/) | [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) |
| Weather Data | [Open-Meteo](https://open-meteo.com) | Open-Meteo API (free) |
| DWD Data | [Deutscher Wetterdienst](https://opendata.dwd.de) | DWD Opendata |
| DWD WMS | [DWD GeoServer](https://maps.dwd.de) | DWD Opendata |
| GUI Framework | [LVGL](https://lvgl.io) | MIT |

See [CREDITS.md](CREDITS.md) for details.

## 📄 License

MIT License – see [LICENSE](LICENSE)

The included weather icons are licensed under **CC BY-SA 4.0** (see [CREDITS.md](CREDITS.md)).

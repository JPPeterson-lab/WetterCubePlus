# 🌦️ WetterCubePlus

Ein kompaktes WLAN-Wetterdisplay auf Basis des **ESP32-S3 N16R8** mit 3,5"-Touchdisplay, DWD-Wetterwarnungen, Pollenflug und Web-Interface.
Dieses Projekt ist die Weiterentwicklung des [WetterCube](https://github.com/JPPeterson-lab/wettercube) und mit KI-Hilfe programmiert.

Wetterdaten kommen kostenlos von [Open-Meteo](https://open-meteo.com) und dem [Deutschen Wetterdienst (DWD)](https://www.dwd.de) – kein API-Key nötig.

> **⚠️ Beta-Version:** Dieses Projekt befindet sich noch in aktiver Entwicklung. Fehler und Breaking Changes sind möglich. / This project is still in active development. Bugs and breaking changes may occur.

---

## ✨ Funktionen

- 🌡️ **Wetterdaten** – Temperatur (Ist & gefühlt), Luftfeuchtigkeit, Luftdruck, UV-Index, Sonnenauf-/-untergang
- 💨 **Wind** – Geschwindigkeit & Richtung
- 🕐 **4-Stunden-Vorhersage** – Icons, Temperatur, Wind pro Slot
- 🌿 **Pollenflug** – 8 Allergene (DWD + Open-Meteo), Stufen heute/morgen/übermorgen
- 🌧️ **Regenwarnung** – Blinkendes Warn-Screen 60 Min. vor erwartetem Regen
- ⚠️ **DWD Wetterwarnungen** – bis zu 5 aktive Warnungen (Level ≥ 2)
- 🗺️ **DWD Warnkarte** – Deutschland-Warnkarte als eigener Screen
- 🖥️ **WebUI** – Alle Einstellungen unter `wettercubeplus.local` (kein Flashen nötig)
- 📡 **Captive Portal** – WLAN-Ersteinrichtung ohne App
- 🔄 **OTA-Update** – Firmware-Update per WebUI über WLAN
- 👆 **Touch-Kalibrierung** – automatisch beim Erststart, jederzeit neu auslösbar

---

## 🎬 Screens

| # | Screen | Inhalt |
|---|---|---|
| 1 | **Hauptscreen** | Wetter aktuell, Uhrzeit, Pollen-Schnellinfo |
| 2 | **Wettervorhersage** | 4×3-Stunden-Slots mit Icon, Temperatur, Wind |
| 3 | **Pollenvorhersage** | Alle 8 Allergene, DWD-Stufen (heute/morgen/übermorgen) |
| 4 | **DWD Warnkarte** | Deutschland-Karte mit aktuellen Warngebieten |
| 5 | **Warnscreen** | Blinkendes Warn-Screen bei Regen- oder Pollenwarnung |

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
5. Standort eingeben – **ohne Umlaute**: `Munchen`, `Koeln`, `Dusseldorf`
6. DWD-Pollenregion für dein Bundesland wählen
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
| **Warnungen** | Regenwarnung ein/aus |
| | Pollenwarnung ein/aus |
| | Pollen-Schwellwert: Gering / Mittel / Hoch / Sehr hoch |
| **DWD-Region** | Bundesland-Auswahl für Wetterwarnungen |
| **Firmware** | OTA-Update: neue Version per WLAN – kein USB nötig |

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
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |

---

## 📡 Datenquellen

| Quelle | Daten |
|---|---|
| [Open-Meteo](https://open-meteo.com) | Wetter, Luftqualität/Pollen – kein API-Key |
| [DWD Opendata](https://opendata.dwd.de) | Pollenflug-Vorhersage, Wetterwarnungen |
| [DWD Warnkarte](https://www.dwd.de) | Deutschland-Warnkarte PNG |

---

## 📄 Lizenz

MIT License – siehe [LICENSE](LICENSE)

---
---

# 🌦️ WetterCubePlus (English)

A compact Wi-Fi weather display based on the **ESP32-S3 N16R8** with a 3.5" touchscreen, DWD weather warnings, pollen data, and a web interface.
This project is the successor to [WetterCube](https://github.com/JPPeterson-lab/wettercube), built and programmed with AI assistance.

Weather data is provided free of charge by [Open-Meteo](https://open-meteo.com) and the [German Weather Service (DWD)](https://www.dwd.de) – no API key required.

---

## ✨ Features

- 🌡️ **Weather data** – Temperature (actual & feels-like), humidity, air pressure, UV index, sunrise/sunset
- 💨 **Wind** – Speed & direction
- 🕐 **4-hour forecast** – Icons, temperature, and wind per time slot
- 🌿 **Pollen** – 8 allergens (DWD + Open-Meteo), levels for today/tomorrow/day after
- 🌧️ **Rain warning** – Flashing warning screen 60 min before expected rainfall
- ⚠️ **DWD weather warnings** – Up to 5 active warnings (level ≥ 2)
- 🗺️ **DWD warning map** – Germany-wide warning map as a dedicated screen
- 🖥️ **WebUI** – All settings at `wettercubeplus.local` (no reflashing needed)
- 📡 **Captive Portal** – Wi-Fi setup without an app
- 🔄 **OTA update** – Firmware update via WebUI over Wi-Fi
- 👆 **Touch calibration** – Automatic on first boot, re-triggerable anytime

---

## 🎬 Screens

| # | Screen | Content |
|---|---|---|
| 1 | **Main screen** | Current weather, time, quick pollen info |
| 2 | **Forecast** | 4×3-hour slots with icon, temperature, wind |
| 3 | **Pollen forecast** | All 8 allergens, DWD levels (today/tomorrow/day after) |
| 4 | **DWD warning map** | Germany map with current warning zones |
| 5 | **Warning screen** | Flashing alert for rain or pollen warnings |

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
6. Select your DWD pollen region
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
| **Warnings** | Rain warning on/off |
| | Pollen warning on/off |
| | Pollen threshold: Low / Moderate / High / Very High |
| **DWD region** | Select federal state for weather warnings |
| **Firmware** | OTA update: install new firmware over Wi-Fi – no USB needed |

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
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |

---

## 📡 Data Sources

| Source | Data |
|---|---|
| [Open-Meteo](https://open-meteo.com) | Weather, air quality/pollen – no API key |
| [DWD Opendata](https://opendata.dwd.de) | Pollen forecast, weather warnings |
| [DWD Warning Map](https://www.dwd.de) | Germany warning map PNG |

---

## 📄 License

MIT License – see [LICENSE](LICENSE)

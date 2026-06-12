# ☁️ WetterCubePlus

Eine kompakte WLAN-Wetterstation auf Basis des **ESP32-S3 N16R8**, die aktuelle Wetterdaten,
Pollenflug und DWD-Wetterwarnungen auf einem 3,5"-Touch-Display (ILI9488, 480×320 px) anzeigt.

Nachfolger des [WetterCube](https://github.com/JPPeterson-lab/wettercube) – jetzt mit größerem
Touch-Display, DWD-Warnkarte und 8 MB PSRAM für erweiterte Möglichkeiten.

Dieses Projekt ist mein zweites selbst gebautes Hardware-Projekt und wird mit KI-Hilfe programmiert.
Aktuell nur auf Deutsch verfügbar.

Wetterdaten kommen kostenlos von [Open-Meteo](https://open-meteo.com) –
Pollenflug und Wetterwarnungen vom [DWD](https://opendata.dwd.de) – kein API-Key nötig.

> ⚠️ **Beta** – Das Projekt befindet sich aktiv in Entwicklung.

---

## ✨ Funktionen

- **Aktuelle Temperatur** (Ist- & gefühlte Temperatur) mit Farbkodierung
- **Luftfeuchtigkeit**, **Luftdruck**, **Windgeschwindigkeit & -richtung**
- **Wetter-Icons** nach WMO-Code (klar, bewölkt, Regen, Schnee, Gewitter, Nebel)
- **Uhrzeit & Datum** per NTP (Zeitzone Europa/Berlin)
- **3h-Forecast** – nächste 4 Stunden mit Icon und Temperatur
- **UV-Index**, Sonnenaufgang & Sonnenuntergang
- **Pollenflug** – 8 Pollenarten vom DWD, regionsbasiert (heute/morgen/übermorgen)
- **DWD-Wetterwarnungen** – aktive Warnungen nach Region, farbkodiert nach Schwere
- **Regen-Warnung** – Roter blinkender Screen, Touch-Quittierung
- **Pollen-Warnung** – Oranger blinkender Screen, konfigurierbarer Schwellwert, Touch-Quittierung
- **Touch-Display** – Navigation zwischen Screens, Warn-Quittierung, Autokalibrierung beim Start
- **Captive-Portal-Setup** – WLAN + Standort + DWD-Region komfortabel per Browser einrichten
- **Automatisches Dimmen** – konfigurierbarer Timeout, erste Berührung weckt ohne Screen-Wechsel
- **Web-Konfiguration** – alle Einstellungen per Browser, kein Flashen nötig
- **WLAN-Zugangsdaten ändern** – ohne Neuflashen, direkt über die Web-UI
- **OTA-Firmware-Update** – neue Version per WLAN einspielen, kein USB nötig

---

## 🎬 Showcase

*Bilder & Video folgen nach erstem stabilen Release.*

---

## ⚡ Web-Installer (kein Arduino IDE nötig)

Die Firmware lässt sich direkt im Browser flashen:

**👉 [WetterCubePlus Web-Installer öffnen](https://jppeterson-lab.github.io/WetterCubePlus/)**

> Funktioniert nur in **Google Chrome** oder **Microsoft Edge** (WebSerial API).
> Das Flashen ist nur einmalig nötig – Updates laufen danach per OTA über WLAN.

### Einrichtung nach dem Flashen

1. Der Cube öffnet das WLAN **`WetterCubePlus-Setup`**
2. Damit verbinden – auf iOS/Android öffnet sich das Portal automatisch, sonst `192.168.4.1` im Browser aufrufen
3. WLAN-Netz und Passwort aus der Liste wählen
4. Stadtname eingeben, DWD-Pollenregion auswählen
5. Speichern → Cube verbindet sich und zeigt sofort das Wetter an

> **Hinweis zu Umlauten:** Städtenamen ohne Umlaute eingeben.
> `Munchen` statt `München` · `Koln` statt `Köln` · `Dusseldorf` statt `Düsseldorf`

---

## 🔧 Touch-Kalibrierung

Beim jedem Start erscheint kurz der Hinweis „Finger halten = Neukalibrierung".
Finger **gedrückt halten** → Kalibrierungsroutine startet, Ecken antippen, gespeichert.
Danach wird die gespeicherte Kalibrierung automatisch geladen – kein erneutes Kalibrieren nötig.

---

## ⚙️ Web-Konfiguration

Erreichbar unter `http://wettercubeplus.local` (mDNS) oder direkt per IP-Adresse.

### Zugang

**Option 1 – mDNS (empfohlen)**
```
http://wettercubeplus.local
```
Funktioniert auf Mac, iPhone, Android und Windows 11. Ältere Windows-Versionen benötigen
[Bonjour](https://support.apple.com/kb/DL999) (Apple, kostenlos).

**Option 2 – IP-Adresse**

Die IP wird nach dem WLAN-Connect im Boot-Screen angezeigt, z. B.:
```
Verbunden: 192.168.1.47
```

### Was sich konfigurieren lässt

| Bereich | Einstellung |
|---|---|
| **Standort** | Stadt (ohne Umlaute), DWD-Pollenregion |
| **Warnungen** | Regen-Warnung ein/aus |
| | Pollen-Warnung ein/aus |
| | DWD-Wetterwarnungen ein/aus |
| | Pollen-Warnschwelle: Gering / Mittel / Hoch / Stark |
| **Display** | Helligkeit: 10–100 % (Slider) |
| | Dimm-Timeout: Aus / 1 / 3 / 5 / 10 Minuten |
| **WLAN** | Zugangsdaten ändern ohne Neuflashen |
| **Firmware** | OTA-Update: neue Version per WLAN einspielen |

### OTA-Update

Unter dem Update-Bereich in der Web-UI:
1. **Auf Updates prüfen** – Cube vergleicht Versionen mit GitHub
2. Bei neuer Version: **Jetzt installieren** – läuft automatisch, Cube startet neu

---

## 🔌 Pinbelegung (ESP32-S3 N16R8)

| Funktion | GPIO |
|---|---|
| TFT SCLK | 14 |
| TFT MOSI | 13 |
| TFT MISO | 12 |
| TFT DC | 2 |
| TFT CS | 15 |
| TFT RST | 16 |
| TFT Backlight (PWM) | 17 |
| Touch CS (XPT2046) | 21 |
| Touch IRQ | 18 |

> Display: **ILI9488** · 3,5" · 480×320 px · SPI · Landscape
> Touch: **XPT2046** resistiv · teilt SPI-Bus mit Display

> ⚠️ **GPIO 33–37 nicht verwenden** – intern für OPI-PSRAM belegt

Vollständige Verkabelung: [WIRING.md](WIRING.md)

---

## 🛠 Verwendete Bibliotheken

| Bibliothek | Zweck |
|---|---|
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Display-Treiber (ILI9488 + XPT2046) |
| [LVGL 8.x](https://lvgl.io) | GUI-Framework |
| [PicoPixel.io](https://picopixel.io) | UI-Design-Tool (LVGL-Export) |
| [ArduinoJson 6.x](https://arduinojson.org) | JSON-Parsing |
| WiFi / HTTPClient / WebServer | Arduino ESP32 Core |
| WiFiClientSecure / HTTPUpdate | OTA + HTTPS-Abrufe |
| Preferences | Einstellungen dauerhaft im Flash speichern |
| ESPmDNS / DNSServer | mDNS + Captive Portal |

---

## 📡 Datenquellen

| Quelle | Daten | API-Key |
|---|---|---|
| [Open-Meteo](https://open-meteo.com) | Wetter, Forecast, UV, Pollen (μg/m³) | Kein Key |
| [DWD OpenData](https://opendata.dwd.de) | Pollenflug-Stufen (8 Allergene, 3 Tage) | Kein Key |
| [DWD Warnungen](https://www.dwd.de) | Aktive Wetterwarnungen nach Region | Kein Key |
| [Open-Meteo Geocoding](https://geocoding-api.open-meteo.com) | Koordinaten aus Stadtname | Kein Key |

---

## 🙏 Credits

| Ressource | Urheber | Lizenz |
|---|---|---|
| Wetter-Icons | [Dovora Weather Icons](https://www.dovora.com/resources/weather-icons/) | [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) |
| Wetterdaten | [Open-Meteo](https://open-meteo.com) | Open-Meteo API (kostenlos) |
| Pollen & Warnungen | [Deutscher Wetterdienst](https://opendata.dwd.de) | Open Data |

---

## 📄 Lizenz

Der Quellcode steht unter der **MIT-Lizenz** – frei nutzbar, veränderbar und weitergabefähig.

Die enthaltenen Wetter-Icons unterliegen der **CC BY-SA 4.0**-Lizenz.

---

## 📋 Devlog

Vollständiger Entwicklungslog mit allen Änderungen pro Version: [DEVLOG.md](DEVLOG.md)

# WetterCubePlus

**Beta** – ESP32-S3 N16R8 Wetterdisplay mit 3,5" Touch-Display

## Hardware

| Komponente | Details |
|---|---|
| MCU | ESP32-S3 N16R8 (16 MB Flash, 8 MB PSRAM) |
| Display | ILI9488 3,5" 480×320, Widescreen/Landscape |
| Touch | XPT2046 (resistiv, SPI) |

### Verkabelung

Siehe [WIRING.md](WIRING.md)

## Features

- Wetterdaten via Open-Meteo (kein API-Key)
- DWD Pollenflug (regionsbasiert)
- DWD Wetterwarnungen
- HTTP-OTA-Update via GitHub Releases
- Web-Installer (ESP Web Tools)
- Captive Portal für WLAN-Erstkonfiguration (deutsch)
- WebUI unter `wettercubeplus.local`
- WLAN-Zugangsdaten nachträglich ändern

## Web-Installer

[JPPeterson-lab.github.io/WetterCubePlus](https://jppeterson-lab.github.io/WetterCubePlus/)

## Bibliotheken (Arduino IDE)

- LovyanGFX
- lvgl 8.x
- ArduinoJson 6.x
- WebServer, ESPmDNS, DNSServer, Preferences, WiFiClientSecure, HTTPUpdate (alle im ESP32 Core)

## Partition Scheme

`Tools → Partition Scheme → 16M Flash (3MB APP/9.9MB FATFS)` oder ein OTA-fähiges Schema für N16R8.

## Credits

Wetter-Icons: [Dovora Weather Icons](https://github.com/dovora/weather-icons) (CC BY-SA 4.0)

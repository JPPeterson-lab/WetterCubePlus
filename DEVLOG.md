# Devlog — WetterCubePlus

---

## 0.1.1-beta — 2026-06-12

### Fixes
- **Touch funktioniert** – Modul "3.5'' TFT SPI 480x320 V1.0" hat separate Touch-SPI-Pins (T_CLK/T_DIN/T_DO ≠ SCK/SDI/SDO); Touch auf SPI3_HOST mit eigenen GPIOs (CLK=6, MOSI=7, MISO=8), `bus_shared=false`
- **Farben korrekt** – ILI9488 braucht `LV_COLOR_16_SWAP=1` + `swap565_t*` im flush_cb; `rgb_order=false` (BGR), `invert=false`
- **Arduino-Präprozessor Raw-String-Bug** – Alle HTML-PROGMEM-Strings nach `webui_html.h` ausgelagert; `.ino`-Präprozessor bricht bei `"` in Raw-Strings ab und interpretiert JS-`function` als C++
- **PicoPixel Font-Include** – Fonts verwenden `"lvgl/lvgl.h"` statt `<lvgl.h>`; `deploy_ui.sh` patcht automatisch via sed
- **Kalibrierung hängt nicht mehr** – Touch-Erreichbarkeitstest vor `calibrateTouch()`; bei keinem Signal: Fehlermeldung + Weiterstart statt hängen
- **Touch-Pins** – Neue Verkabelung: T_CLK→GPIO6, T_DIN→GPIO7, T_DO→GPIO8

### Technische Notizen
- lv_conf.h: `LV_COLOR_16_SWAP 1` für ILI9488+LovyanGFX (vorher 0 für Arduino_GFX/SLS-Workflow)
- Panel_ILI9488 in LovyanGFX: `rgb_order=false` (ILI9488 default = BGR)

### Erstes getestetes Release
- Touch-Kalibrierung mit Dauerdruck-Erkennung funktioniert
- Farben korrekt auf ILI9488 480×320
- Boot-Screen mit PicoPixel-UI sichtbar
- Captive Portal erreichbar
- Bins für Web-Installer erstellt

---

## 0.1.0-beta — 2026-06-12

**Erster Commit / Grundkonstrukt**

### Hardware
- ESP32-S3 N16R8 (16 MB Flash, 8 MB OPI-PSRAM)
- ILI9488 3,5" SPI-Display, 480×320 px, Landscape/Widescreen
- XPT2046 resistiver Touchscreen, teilt SPI-Bus mit Display

### Pinbelegung
- TFT: SCLK=14, MOSI=13, MISO=12, DC=2, CS=15, RST=16, BL=17
- Touch: CS=21, IRQ=18

### Implementiert
- Display-Ansteuerung über LovyanGFX (Panel_ILI9488, SPI2_HOST, Touch_XPT2046, Light_PWM)
- LVGL 8.x UI erstellt mit PicoPixel.io
- PSRAM-optimierter LVGL-Buffer: `ps_malloc()`, 40 Zeilen × 480 px
- **Touch-Autokalibrierung:** Finger beim Start gedrückt halten → Neukalibrierung; gespeichert in Preferences `touch_cal`
- **PicoPixel UI integriert:** Boot-Screen (`screenboot`) mit `labelstatus` + `barwifi` (Fortschrittsbalken), Hauptscreen (`screen_1`) mit `labeltime`
- `ui_tick()` in `loop()` eingebunden
- **Captive Portal** für WLAN-Erstkonfiguration (vollständig deutsch)
  - WLAN-Scan als Dropdown
  - DWD-Pollenregion wählbar (16 Regionen)
  - Hinweis auf Umlaute
  - Speichert ssid/pass/location/dwd_region → Neustart
- **WebUI** unter `wettercubeplus.local` (mDNS)
  - Standort, DWD-Region
  - Regen-/Pollen-/DWD-Warnungen ein/aus
  - Pollen-Warnschwelle: Gering/Mittel/Hoch/Stark
  - Helligkeit (Slider), Dimm-Timeout
  - WLAN-Zugangsdaten ändern ohne Neuflashen (`/wlan`)
  - OTA-Versionscheck + Update per WLAN (`/ota_check`, `/ota_update`)
- **HTTP-OTA** via GitHub Releases (HTTPUpdate + WiFiClientSecure + setInsecure())
- **Web-Installer** in `docs/` (ESP Web Tools), GitHub Pages aktiv
  - `docs/manifest.json` mit ESP32-S3 Offsets (0 / 32768 / 57344 / 65536)
  - `docs/version.json` für OTA-Check
- **Open-Meteo Wetter** (current + hourly + daily), HTTPS, WiFiClientSecure
  - Temperatur, gefühlte Temp, Luftfeuchte, Wind, Luftdruck, WMO-Code
  - UV-Index, Sonnenauf-/-untergang
  - 4×3h-Forecast mit Temperatur + WMO-Code
  - Regenwarnung: WMO ≥ 51 in nächsten 2h
- **Open-Meteo Air Quality** – Pollen in μg/m³ (Birke, Gräser, Erle, Beifuß, Ambrosia)
- **DWD Pollenflug** (`opendata.dwd.de/…/s31fg.json`)
  - 8 Allergene: Birke, Hasel, Erle, Esche, Gräser, Roggen, Beifuß, Ambrosia
  - Stufen 0–3 als Float via `parseDwdWert()` (parst "0-1", "2-3", "-1" etc.)
  - Filter nach `region_id`
  - JSON-Keys ohne Umlaute: `Graeser`, `Beifuss`
- **DWD Wetterwarnungen** (`warnapp_gemeinden/json/warnings.json`)
  - Max. 5 aktive Warnungen, Level ≥ 2
  - 32 KB DynamicJsonDocument (PSRAM ermöglicht das)
- **Display-Dimmen:** konfigurierbarer Timeout, Touch weckt ohne Screen-Wechsel
- `deploy_ui.sh` für PicoPixel-Export (`src/ui/` → `ui/`)
- Preferences-Namespace: `wcp`

### Boot-Ablauf mit Fortschrittsbalken
| Schritt | Fortschritt |
|---|---|
| LVGL + UI init | – |
| WLAN-Verbindung | 10–30 % |
| NTP-Sync | 45 % |
| WebUI + mDNS | 55 % |
| Geocoding | 65 % |
| Wetterdaten | 70–80 % |
| Pollen | 80–90 % |
| DWD-Warnungen | 90–100 % |
| Hauptscreen laden | – |

### Technische Notizen
- LV_COLOR_16_SWAP = 0 (LovyanGFX macht swap intern via `swap565_t*` im flush_cb)
- PicoPixel erzeugt keinen `#if LV_COLOR_16_SWAP`-Block – kein Patch nach Export nötig
- `pp_anim_stop_timelines_for_deleted_tree()` muss in `actions.c` definiert sein (PicoPixel-Pflicht)
- PicoPixel Font-Dateien includieren `"lvgl/lvgl.h"` statt `<lvgl.h>` → Compile-Fehler; `deploy_ui.sh` patcht das automatisch via `sed`
- **Modul "3.5'' TFT SPI 480x320 V1.0"**: Touch-Pins T_CLK/T_DIN/T_DO sind SEPARATE Drähte, nicht geteilt mit Display-SCK/SDI/SDO → Touch-SPI auf SPI3_HOST, eigene GPIOs (6/7/8)
- ILI9488 Farb-Bug: `swap565_t*` Cast im flush_cb ergibt lila Farben bei LV_COLOR_16_SWAP=0 → Fix: `uint16_t*` Cast
- Arduino-Präprozessor versteht Raw-String-Literals in `.ino` nicht: bricht bei `"` innerhalb des Strings ab und sieht danach JS-`function` als C++ → `'function' does not name a type`; Fix: alle HTML-PROGMEM-Strings in `webui_html.h` ausgelagert
- Nach `lv_scr_load_anim`: 600 ms `lv_timer_handler()`-Loop vor HTTP-Calls (Lessons Learned WetterCube)
- DWD Pollenflug: JSON-Keys OHNE Umlaute – `Graeser` nicht `Gräser`, `Beifuss` nicht `Beifuß`
- HTTPS immer: `WiFiClientSecure sc; sc.setInsecure();` + `http.begin(sc, url)`
- GPIO 33–37 intern für OPI-PSRAM gesperrt beim N16R8

### Bekannte Einschränkungen / Noch offen
- UI-Screens noch nicht vollständig (nur Boot + Hauptscreen-Skeleton mit Uhrzeit)
- Warnscreen-Screens (rot/orange blinkend) noch nicht implementiert
- DWD-Warnkarte als visuelle Darstellung noch ausstehend
- Wetter-Icons noch nicht eingebunden
- Pollen-Forecast-Screen (heute/morgen/übermorgen) noch ausstehend
- Web-Installer lokal noch nicht getestet (keine .bin vorhanden)

---

<!-- neue Einträge oben einfügen -->

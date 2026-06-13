# Changelog

## v0.2.1-beta (2026-06-13)

### Neu
- Home-Button auf allen Screens: kehrt direkt zu Screen 1 zurück
- Weißes UI-Theme in PicoPixel umgesetzt (reduziert Spiegelungen bei Glasscheibe)

### Fixes
- OTA-Update: falscher Dateiname im Download-Pfad korrigiert (`firmware.bin` statt `WetterCubePlus-X.X.X.bin`)
- OTA-Update: `HTTPC_FORCE_FOLLOW_REDIRECTS` für GitHub CDN-Redirects

---

## v0.2.0-beta (2026-06-13)

### Neu
- DWD Warnkarte: Deutschland-Warnkarte als eigener Screen (PNG-Dekodierung via lodepng in PSRAM)
- Warnscreen-Hintergrund blinkt statt Icon/Text (bessere Sichtbarkeit)
- deploy_ui.sh: automatischer Fix für PicoPixel ARGB-Farbfehler (`lv_color_hex(0xffRRGGBB)` → `0xRRGGBB`)
- Helles Theme: weißer Hintergrund optional konfigurierbar (reduziert Spiegelungen bei Glas)

### Fixes
- **Helligkeit**: Backlight-PWM jetzt über ESP-IDF LEDC-API direkt (kein Arduino-Wrapper) – zuverlässig auf allen GPIO
- **Warnkarte**: lodepng nutzt jetzt PSRAM-Allokation (`heap_caps_malloc`) statt LVGL-Heap (war 128 KB, zu klein für PNG-Dekodierung)
- **Warnkarte Download**: Stream-Loop durch robuste `readBytes`-Schleife ersetzt – verhindert Abbruch bei HTTPS/Chunked-Encoding
- **Regenwarnung**: Prüft jetzt korrekt aktuelle + nächste Stunde (war off-by-one)
- **Pollenwarnung**: Schwellen-Vergleich korrigiert (`>` statt `>=`) – „hoch" warnt nicht mehr bei Schwelle „hoch"
- **Farben**: `LV_COLOR_16_SWAP 1` wiederhergestellt (war versehentlich auf 0 gesetzt → falsche Farben)
- **Screen-Schwarzhintergrund wirkte bläulich**: PicoPixel exportiert `0xff000000` als ARGB, LVGL interpretierte `0xFF` als Rot-Kanal

### Intern
- Light_PWM aus LovyanGFX-Klasse entfernt; Backlight vollständig manuell über LEDC gesteuert
- LVGL-Screen-Übergang: FADE_IN durch NONE ersetzt (Crash-Fix bei schnellem Screen-Wechsel)

---

## v0.1.1-beta (2026-06-12)

### Neu
- PicoPixel UI vollständig integriert: Boot-Screen, Hauptscreen, Wettervorhersage, Pollenvorhersage, Warnkarte-Screen, Regenwarnung, Pollenwarnung
- Alle Wetter-Icons als RGB565A8 eingebettet (PSRAM, LV_COLOR_16_SWAP-kompatibel)
- Touch XPT2046 auf eigenem SPI-Bus (SPI3_HOST), keine Kollision mit Display

### Fixes
- LVGL FADE_IN Crash behoben
- LV_COLOR_16_SWAP + swap565_t korrekt konfiguriert

---

## v0.1.0-beta (2026-06-11)

- Erstes Release
- Grundstruktur: LovyanGFX + LVGL 8, PSRAM-Buffer, WebUI, Captive Portal, OTA
- Datenquellen: Open-Meteo, DWD Pollenflug, DWD Warnungen
- Web-Installer über GitHub Pages

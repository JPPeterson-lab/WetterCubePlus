# Entwicklungs-Log

## 2026-06-13 – v0.2.2-beta

### OTA CDN-Redirect-Fix (zweite Iteration)
`HTTPC_FORCE_FOLLOW_REDIRECTS` folgt Cross-Host-Redirects prinzipiell, aber `httpUpdate` hat intern einen eigenen HTTPClient der die Redirect-Policy nicht übernimmt — daher „connection refused" auf dem CDN. Lösung: Redirect manuell auflösen mit einem separaten HTTPClient (Follow Redirects deaktiviert, `GET` → prüfe 301/302/307/308, lese `Location`-Header). Den aufgelösten CDN-URL dann direkt an `httpUpdate.update()` übergeben. `httpUpdate` selbst sieht nur den direkten CDN-Link und braucht keine Redirects mehr zu folgen.

---

## 2026-06-13 – v0.2.1-beta

### Home-Button
PicoPixel platziert den Home-Button auf allen Screens als `labelbuttonhome`, `labelbuttonhome_1`, `labelbuttonhome_2`. Der Button wird in screens.c erstellt, aber PicoPixel generiert keine Event-Callbacks. Lösung: `cbHome` Callback im .ino registriert, der `loadScreen(SCREEN_ID_SCREEN_1)` aufruft – für alle drei Instanzen per `REG_CB`.

### OTA-Fix
GitHub Release-Assets werden unter dem Dateinamen hochgeladen wie sie heißen (`firmware.bin`). Der OTA-Code hatte einen hardcodierten Pfad `WetterCubePlus-{version}.bin` – nicht gefunden. Zusätzlich: GitHub Releases leiten auf ein CDN (objects.githubusercontent.com) um. `HTTPC_STRICT_FOLLOW_REDIRECTS` blockiert Cross-Host-Redirects; `HTTPC_FORCE_FOLLOW_REDIRECTS` folgt auch domain-übergreifend.

---

## 2026-06-13 – v0.2.0-beta

### DWD Warnkarte
Die Warnkarte war das schwierigste Problem dieser Version. Das PNG wird von DWD als HTTPS-Stream geliefert. Drei Probleme mussten gelöst werden:

1. **Download bricht nach ~6 KB ab**: `stream->available()` kehrt bei HTTPS/Chunked-Encoding kurzzeitig mit 0 zurück, obwohl noch Daten kommen. Lösung: `readBytes` in 4-KB-Chunks mit `stream->connected()` als Abbruchbedingung.

2. **lodepng schlägt fehl (Fehler 64 / OOM)**: lodepng verwendet intern `lv_mem_alloc`, der Pool ist aber nur 128 KB groß. Eine 600×600 PNG benötigt ~1,5 MB für die RGBA-Ausgabe. Lösung: lodepng auf `heap_caps_malloc(MALLOC_CAP_SPIRAM)` umgestellt.

3. **`LV_IMG_CF_RAW_ALPHA` wird vom LVGL Built-in-Decoder abgelehnt** (Enum-Wert 2 < `CF_BUILT_IN_FIRST`=4). Lösung: PNG selbst in `fetchDwdWarnkarte()` dekodieren, RGBA→RGB565A8 konvertieren, Descriptor als `LV_IMG_CF_RGB565A8` setzen – LVGL braucht keinen PNG-Decoder mehr zur Darstellung.

### Backlight / Helligkeit
Drei Iterationen bis zur Lösung:
- **LovyanGFX Light_PWM**: funktionierte nicht zuverlässig, da `tft.begin()` intern Brightness auf 0 setzt und der Aufruf-Zeitpunkt kritisch ist
- **Arduino `ledcAttach`/`ledcWrite`**: lieferte ESP_OK, brachte aber keine sichtbare Änderung
- **ESP-IDF LEDC direkt** (`ledc_timer_config` + `ledc_channel_config`): funktioniert, sobald die GPIO-Verdrahtung stimmt (Kabel war das eigentliche Problem)

Debugging-Trick: setBrightness(0) für 300 ms direkt nach tft.begin() – sichtbares Dunkelwerden bestätigt funktionierende LEDC-Konfiguration.

### PicoPixel Farbfehler
PicoPixel exportiert Farben als `lv_color_hex(0xffRRGGBB)` (ARGB8888-Format). LVGL's `lv_color_hex()` nimmt aber nur 24-Bit RGB: `0xff000000` → r=255, g=0, b=0 → **Rot**. In Kombination mit `LV_COLOR_16_SWAP=1` und ILI9488-Byte-Reihenfolge wirkte der schwarze Hintergrund bläulich. Fix: `0xff`-Prefix per `sed` entfernen, in `deploy_ui.sh` automatisiert.

### Bekannte Einschränkungen
- Regionale DWD-Warnkarten: DWD bietet keine bundesland-spezifischen PNGs über die öffentliche API an; es wird die Deutschlandkarte angezeigt
- Skalierung: Warnkarte wird auf Display-Breite skaliert (fit-to-screen), Navigation-Buttons bleiben frei

---

## 2026-06-12 – v0.1.1-beta

### PicoPixel UI-Integration
PicoPixel.io exportiert LVGL 8 kompatiblen Code. Zwei Hürden:
- Generierte `images/*.c` werden von Arduino IDE doppelt kompiliert (direkt + via `#include` in images.c) → Lösung: `.c` → `.inc` umbenennen, deploy_ui.sh automatisiert das
- `lv_scr_load_anim(FADE_IN, 200ms)` crasht wenn `loadScreen()` innerhalb von 200 ms zweimal aufgerufen wird → `LV_SCR_LOAD_ANIM_NONE`

### LV_COLOR_16_SWAP
ILI9488 + LovyanGFX + LVGL: Das richtige Setting ist `LV_COLOR_16_SWAP 1` mit `swap565_t` in `disp_flush`. LVGL speichert Farben dann im Big-Endian RGB565-Format, LovyanGFX sendet die Bytes unverändert an die ILI9488 – korrekte Farbanzeige.

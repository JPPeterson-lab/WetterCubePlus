# Entwicklungs-Log

## 2026-06-13 – v0.2.4-beta

### OTA – Endlösung (dritte Iteration)

**Problem**: ESP32-S3 kann sich nicht mit `github.com:443` verbinden — rc=-1 in unter 100 ms, also kein Timeout sondern sofortige Ablehnung. `raw.githubusercontent.com` (für version.json) funktioniert dagegen problemlos.

**Ursache**: `github.com` nutzt eine TLS-Konfiguration (Cipher Suites / Zertifikatskette) die mbedTLS auf dem ESP32 nicht verarbeiten kann. `githubusercontent.com` und `github.io` sind davon nicht betroffen.

**Zweites Problem**: In der v0.2.2/0.2.3-Implementierung wurde `server.send()` VOR dem OTA-Download aufgerufen. Das hält die WebServer-TCP-Verbindung offen und konkurriert mit dem anschließenden `WiFiClientSecure`-Socket — führte ebenfalls zu rc=-1.

**Lösung**:
1. Firmware-Download-URL auf **GitHub Pages** umgestellt: `https://jppeterson-lab.github.io/WetterCubePlus/firmware/firmware.bin` — feste URL, kein Redirect, ESP32-kompatibles TLS
2. `httpUpdate`-Library komplett entfernt, ersetzt durch `HTTPClient` + `Update.writeStream()` direkt (identisch mit WetterCube-Implementierung die funktioniert)
3. `server.send()` erst NACH erfolgreichem Download — keine offene WebServer-Verbindung während des Downloads
4. Version in version.json bleibt bei `0.2.4-beta`, Firmware in `docs/firmware/firmware.bin` wird bei jedem Release überschrieben

**Workflow für künftige Releases**:
- FIRMWARE_VERSION bumpen
- version.json bumpen  
- Arduino IDE: Sketch → Exportiere kompiliertes Binary
- `docs/firmware/firmware.bin` (+ bootloader/partitions/boot_app0) ersetzen
- Pushen — GitHub Pages deployed automatisch, OTA funktioniert nach ~1 Minute

---

## 2026-06-13 – v0.2.2-beta / v0.2.3-beta

### OTA CDN-Redirect-Fix (zweite Iteration, nicht erfolgreich)
Versuch: Redirect manuell auflösen mit separatem HTTPClient, dann direkten CDN-URL an `httpUpdate` übergeben. Scheiterte weil bereits der erste GET auf github.com (rc=-1) fehlschlug — nicht der Redirect war das Problem sondern github.com selbst.

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

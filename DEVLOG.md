# Entwicklungs-Log

## 2026-06-21 – v0.3.2-beta

### Windpfeil Transparenz-Fix

`lv_obj_set_style_bg_opa(windBg, 160, 0)` war zu opak (~63%) — Radarkarte darunter kaum sichtbar. Auf 90 (~35%) reduziert, Karte bleibt gut lesbar, Pfeil trotzdem klar erkennbar.

---

## 2026-06-15 – v0.3.1-beta

### Standort-Marker auf Radarkarte

Roter Punkt auf dem Bundesland-Radar zeigt den konfigurierten Standort. Pixel-Position berechnet aus `cfg.lat`/`cfg.lon` und der BBOX des Bundeslandes:

```
px = (cfg.lon - bb->minLon) / (bb->maxLon - bb->minLon) * 480
py = (1 - (cfg.lat - bb->minLat) / (bb->maxLat - bb->minLat)) * 270
```

Marker ist ein LVGL-Objekt (`lv_obj`) mit `LV_RADIUS_CIRCLE`, 10×10 px, rot, im Z-Order über dem Radarbild. Wird beim ersten erfolgreichen Radar-Download positioniert und bei jedem Update aktualisiert. Vor dem ersten Download versteckt (`LV_OBJ_FLAG_HIDDEN`).

---

## 2026-06-15 – v0.3.0-beta

### ScreenSunMoon – neuer 5. Screen

Neuer Screen mit UV-Index, Sonnenaufgang und -untergang. Navigation ist jetzt eine vollständige Runde über 5 Screens.

**Datenquelle UV:** `uv_index_max` aus Open-Meteo Daily-API – bereits im Wetter-Fetch vorhanden, kein zusätzlicher API-Call nötig. Stundenwerte wären für UV nicht sinnvoll (tags 0, abends 0).

**Datenquelle Sunrise/Sunset:** `sunrise`/`sunset` aus Open-Meteo Daily-API, bereits als `wetter.sunrise`/`wetter.sunset` (Format `HH:MM`) im Struct.

**Navigation:** PicoPixel vergibt Button-Namen inkrementell je Export. Der neue Screen brachte `labelbuttonforward_4` für ScreenWarnkarte1 (vorher `_3`) und `labelbuttonforward_2` + `button_1` für SunMoon. Alle REG_CB-Einträge entsprechend angepasst.

---

## 2026-06-14 – v0.2.16-beta

### Nachtmodus + Pollen-Fix + Radar-Fallback

**Nachtmodus:** Zeitbasiertes Dimmen unabhängig vom Inaktivitäts-Timeout. Von/Bis in 15-Min-Schritten, eigene Nacht-Helligkeit (0–50 %). Mitternachts-Überlapp korrekt behandelt (`night_from >= night_to` → OR-Logik). Konfigurierbar in der WebUI unter Display.

**Touch-Blinken im Nachtmodus:** `touch_read()` restaurierte bei jedem Touch bedingungslos die volle Helligkeit via `setBrightness(cfg.brightness)`. Im Nachtmodus ist `dimmed = true` dauerhaft gesetzt → Touch setzte zurück auf hell, Loop dimmed wieder → sichtbares Blinken. Fix: Touch-Reset nur bei `!cfg.night_mode`.

**Pollen-Schwellwerte:** Abweichung zwischen WetterCube (≤10/≤30/≤100 Grains/m³) und WetterCubePlus (altes System: <25/<75/<150) führte zu unterschiedlichen Anzeigen bei denselben Daten. Angeglichen auf WetterCube-Schwellen.

**Radar-Fallback:** DWD GeoServer gibt für Layer-Kombination `KV_VG250_BUNDESLAENDER_2020 + Niederschlagsradar` gelegentlich `internalError: Unable to obtain connection` zurück (Datenbankproblem serverseitig). Content-Type-Prüfung (`application/vnd.ogc.se_xml`) erkennt den Fehler; automatischer Fallback auf nur `dwd:Niederschlagsradar`. Radarkarte erscheint ohne Bundeslandgrenzen statt gar nicht.

---

## 2026-06-14 – v0.2.15-beta

### Eigene Partitionstabelle – mehr Platz für Firmware

Bei 88 % Flash-Auslastung (Slot à ~1,5 MB) war absehbar, dass weitere Features (z.B. erneute Warnkarte, mehr Icons) den Slot sprengen. Lösung: eigene `partitions.csv` im Sketch-Verzeichnis.

**Neues Layout (16 MB Flash):**

| Bereich | Offset | Größe |
|---|---|---|
| nvs | 0x9000 | 20 KB |
| otadata | 0xe000 | 8 KB |
| app0 (ota_0) | 0x10000 | 4 MB |
| app1 (ota_1) | 0x410000 | 4 MB |
| spiffs | 0x810000 | ~8 MB |

Arduino IDE liest `partitions.csv` im Sketch-Ordner automatisch ein. In der IDE einmalig **Tools → Partition Scheme → Custom** wählen.

**Warum kein Nachteil:** SPIFFS (9,9 → 8 MB) wird nicht genutzt – alle Bilder sind als `.inc`-Arrays in der Firmware eingebettet. OTA funktioniert weiterhin mit beiden Slots. Nur die erste Installation der neuen Partitionstabelle erfordert USB-Flash – danach wieder vollständig OTA-fähig.

---

## 2026-06-14 – v0.2.14-beta

### LV_COLOR_16_SWAP + build_opt.h

**Problem:** Alle Farben im UI waren byte-vertauscht (blau erschien orange, rot erschien cyan etc.). Erst nach Hinzufügen farbiger Temperaturlabels auf dem Forecast-Screen fiel das Problem auf — schwarze Texte sind von Byte-Swap nicht betroffen, bunte Farben schon.

**Ursache:** `disp_flush` nutzt `lgfx::swap565_t*` als Pointer-Typ für `tft.writePixels()`. Das signalisiert LovyanGFX: „Daten sind bereits im Display-Byte-Order, nicht nochmal swappen." LVGL muss die Bytes also selbst vorher tauschen — das erfordert `LV_COLOR_16_SWAP=1`. Die globale `lv_conf.h` hatte `LV_COLOR_16_SWAP=0` als Default, und es fehlte eine projektspezifische `build_opt.h`.

**Fix:** `build_opt.h` im Sketch-Verzeichnis angelegt:
```
-Os
-DLV_COLOR_16_SWAP=1
-DLV_MEM_SIZE=65536
```
Arduino IDE liest diese Datei beim Kompilieren als zusätzliche Compiler-Flags ein. Damit ist `LV_COLOR_16_SWAP=1` projekt-lokal gesetzt, ohne die globale `lv_conf.h` zu verändern.

**Merkregel:** `swap565_t` in `disp_flush` → immer `LV_COLOR_16_SWAP=1`. Ohne `swap565_t` (z.B. WetterCube mit `draw16bitRGBBitmap`) → `LV_COLOR_16_SWAP=0`.

---

## 2026-06-14 – v0.2.12-beta

### Radarkarte: Download-Fix + Layout

**Problem 1: Truncated PNG (lodepng Fehler 28)**
Der DWD WMS-Server liefert die Karte ohne `Content-Length`-Header (Chunked-Transfer). Die bisherige Stream-Schleife lief daher immer bis zum konfigurierten Timeout (erst 20s, dann 30s) — die Verbindung schließt sich clientseitig, nicht serverseitig. Resultat: die letzten ~1 KB des PNG fehlen, lodepng kann es nicht dekodieren.

Fix: `http.getString()` statt manueller Stream-Schleife. `getString()` wartet intern korrekt auf das Ende des Chunked-Streams und gibt erst dann zurück.

**Problem 2: Bild überdeckte Navigationsbuttons**
Buttons im `screenwarnkarte1`-Screen sitzen bei Y=273, Höhe 40px. Das angeforderte Bild war 480×280 → letzten 7px überlagerten die Buttons. Fix: `HEIGHT=270`, `LV_ALIGN_TOP_MID` statt CENTER.

**Aktualisierungsintervall:** 30 → 10 Minuten. DWD Niederschlagsradar aktualisiert alle 5 Minuten; 10 Minuten ist ein guter Kompromiss zwischen Aktualität und Download-Last (~30 KB pro Abruf).

---

## 2026-06-14 – v0.2.11-beta

### DWD WMS Niederschlagsradar (bundeslandbezogen)

Die bisherige Warnkarte zeigte eine fest codierte Deutschland-PNG vom DWD — auf einem 480×320 Display zu klein um Details zu erkennen.

**Neue Lösung: DWD WMS GetMap-Request** mit Bundesland-BBOX.

Der DWD GeoServer (`maps.dwd.de/geoserver/dwd/ows`) bietet den Layer `niederschlagsradar` als WMS-Dienst. Über den `BBOX`-Parameter lässt sich ein beliebiger Kartenausschnitt in beliebiger Pixelgröße abrufen — kein separates Kartenbild-Asset nötig, der Server rendert alles serverseitig.

**BBOX-Tabelle:** Für jede der 15 DWD-Pollenregionen sind die Bounding-Box-Koordinaten (EPSG:4326: minLon, minLat, maxLon, maxLat) hinterlegt. Bayern hat 4 Subregionen (71/72/73/74) die alle auf Bayern gesamt (9.0/47.3/13.9/50.6) zeigen. Die vorhandene `cfg.dwd_region`-Einstellung aus dem Setup wird direkt genutzt — kein neues Konfigurationsfeld nötig.

**WMS-Anfrage:** `WIDTH=480&HEIGHT=280` — passt exakt ins Display (320px − 40px Navigationsbuttons). Das Bild wird 1:1 ohne Zoom angezeigt (`lv_img_set_zoom(img, 256)`).

**Download-Logik:** Identisch mit bisheriger Warnkarte (HTTPClient → PSRAM-Buffer → lodepng → RGB565A8-Konvertierung). Aktualisierung alle 30 Minuten.

---

## 2026-06-13 – v0.2.10-beta

### Pollen-Architektur: Open-Meteo stündlich für Screen 1 und Warnung

Screen 1 zeigte bisher die DWD-Tageswerte für Pollen (einmal täglich aktualisiert, 8 Allergene). Das Problem: die Werte repräsentieren den Tagesdurchschnitt, sind nicht live, und stimmen oft nicht mit der aktuellen Belastung überein.

**Neue Architektur:**
- **Screen 1 Top-3** und **Pollenwarnung** nutzen jetzt Open-Meteo Stundenwerte für die *nächste* Stunde (`h+1`). `fetchOpenMeteoPollen()` liest nun `min(tm_hour + 1, 23)` statt `tm_hour`.
- **3-Tage-Pollenforecast** (`screenforecastpollen`) bleibt auf DWD-Tageswerten — die sind für mehrtägige Vorhersage geeigneter.

**Umrechnung Open-Meteo → DWD 0–3 Skala:** Open-Meteo liefert Körner/m³, `pollenText()` und `pollenColor()` erwarten die DWD 0–3 Skala. Neue Funktion `openMeteoToDwd(grains)`:
- 0 Körner → 0.0 (keine)
- 1–24 → 1.0 (gering)
- 25–74 → 2.0 (mittel)
- 75–149 → 2.5 (hoch)
- ≥ 150 → 3.0 (stark)

Schwellen basieren auf europäischen Pollenbelastungs-Standards. Open-Meteo hat 5 Allergene (Birke, Erle, Gräser, Beifuß, Ambrosia); Hasel, Esche, Roggen sind nur in den DWD-Werten vorhanden und bleiben dort.

---

## 2026-06-13 – v0.2.9.1-beta

### DWD Pollen JSON-Buffer zu klein
`DynamicJsonDocument(8192)` reichte nicht für die gefilterte DWD-Antwort — `deserializeJson` gab `NoMemory` zurück, alle Pollenwerte blieben auf -1.0f ("--"). Buffer auf 16384 Bytes erhöht. ArduinoJson-Filter reduziert die Datenmenge (nur `region_id` + `Pollen` pro Region), aber bei ~18 Regionen × 8 Allergene × 3 Tage kommt trotzdem mehr als 8 KB zusammen.

---

## 2026-06-13 – v0.2.9-beta

### Temperatur-Farbcodierung
WetterCubePlus hatte andere Schwellen als WetterCube: blau erst unter 0°C statt unter 8°C, rot erst ab 25°C statt ab 24°C. Angeglichen auf WetterCube-Werte: `< 8` blau · `< 15` grün · `< 24` gelb · `≥ 24` rot.

---

## 2026-06-13 – v0.2.8-beta

### Wetter-Icons vollständig

PicoPixel unterstützt keinen direkten Asset-Import ohne zugehörigen Screen. Lösung: einen „Lagerscreen" anlegen der nie in die Navigation eingebunden wird — er dient nur als Container damit PicoPixel die Icons als `.inc`-Dateien exportiert. Im `.ino` wird `objects.lagerscreen` nie über `loadScreen()` oder `lv_scr_load()` aufgerufen → bleibt unsichtbar.

`wmoZuImage()` nutzt jetzt alle 5 Icons korrekt:
- WMO 0–1: `day_clear`
- WMO 2–48: `overcast` (Bewölkung, Nebel)
- WMO 51–82: `rain`
- WMO 71–86: `snow`
- WMO 95–99: `thunder`

---

## 2026-06-13 – v0.2.7-beta

### Boot-Screen Fix

**Problem 1**: Boot-Screen war nie sichtbar. `ui_init()` (PicoPixel-generiert) erstellt alle Screens in dieser Reihenfolge: `screen_1`, `screenforecastpollen`, `screenwarnkarte1`, `screenboot`, `screenwarnung`, `screenwarnungpollen`, `screenforecastwetter` — der zuletzt erstellte Screen ist aber NICHT automatisch der aktive. LVGL setzt den ersten erstellten Screen als aktiven Display: `screen_1`. Fix: `lv_scr_load(objects.screenboot)` explizit nach `ui_init()` aufrufen.

**Problem 2**: `lv_timer_handler()` einmalig aufgerufen reicht nicht um ein 480×320-Display vollständig zu rendern. Fix: `lvgl_flush(ms)` — ruft `lv_timer_handler()` in einer Schleife für mindestens 80ms auf.

**Problem 3**: `aktualisiereUI()` wurde nach `loadScreen(SCREEN_ID_SCREEN_1)` aufgerufen → Hauptscreen erschien kurz leer. Fix: `aktualisiereUI()` vorher aufrufen, dann erst Screen laden.

**Problem 4**: Kästchen-Zeichen `□` nach Boot-Meldungen. Ursache: `…` (U+2026, Ellipsis) ist nicht in der PicoPixel-eingebetteten Bitmap-Font enthalten. Fix: durch drei normale Punkte `...` ersetzt.

---

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

# Changelog

## v0.6.1-beta (2026-07-15)

### Neu
- **4 neue Wetter-Icons:** `day_partial_cloud`, `night_full_moon_partial_cloud`, `sleet` (Schneeregen), `fog` (Nebel)
- **Tag/Nacht-Icons:** Hauptscreen-Icon wechselt automatisch zwischen Tag- und Nacht-Variante (`is_day` aus Open-Meteo)
- **Verfeinerte WMO-Zuordnung:** Nebel (WMO 45–48) und Schneeregen (WMO 58–67) haben jetzt eigene Icons statt Fallback auf `rain`/`overcast`

---

## v0.6.0-beta (2026-07-12)

### Neu
- **Menü-Screen: Helligkeitsregler** – Arc-Slider direkt am Gerät, Änderung sofort aktiv und wird gespeichert
- **Menü-Screen: Regenwarnung an/aus** – Toggle-Switch, Zustand wird persistiert
- **Menü-Screen: Pollenwarnung an/aus** – Toggle-Switch, Zustand wird persistiert
- **Menü-Screen: DWD-Warnungen an/aus** – Toggle-Switch; beim Deaktivieren verschwindet der Warn-Indikator auf screen_1 sofort

---

## v0.5.4-beta (2026-07-12)

### Neu
- **Menü-Screen (ScreenMenu):** Neuer Screen erreichbar über `LabelButtonMenu` auf screen_1 – `LabelButtonMenu_2` führt zurück zu screen_1
- **Theme-Wechsel (experimentell):** `LabelSwitchTheme` im Menü wechselt zwischen den Themes „Light" und „Summer" – Grundlage für das spätere vollständige Theme-System, aktuell noch in Entwicklung

---

## v0.5.3-beta (2026-07-11)

### Neu
- **Home-Button auf ScreenSunMoon:** Antippen bringt zurück zu Screen 1 (war nicht verdrahtet)
- **Home-Button auf ScreenForecastPollenHour:** Antippen bringt zurück zu Screen 1 (war nicht verdrahtet)
- **Tagestemperatur Min/Max auf ScreenSunMoon:** Tief- und Höchsttemperatur des Tages (`LabelTempminValue` / `LabelTempmaxValue`) mit temperaturbasierter Farbkodierung (blau/grün/gelb/rot)

---

## v0.5.2-beta (2026-06-29)

### Fixes
- **Ampel-API `active`-Berechnung:** Kaskadierter Vergleich über `min`-Werte statt min+max-Bereiche — keine Lücken mehr zwischen Schwellen, Ampel zeigt immer eine Farbe solange Temperatur ≥ gruen_min

---

## v0.5.1-beta (2026-06-28)

### Fixes
- **Pollenforecast stündlich:** Slots klemmen nicht mehr auf 23h — API-Index läuft jetzt über die Tagesgrenze (0–47), Anzeige-Stunde korrekt mit `% 24` berechnet
- **Ampel-Addon DWD-Logik:** `dwd_warning` im API-Response spiegelt jetzt exakt den Blink-Zustand des screen_1-Warn-Buttons — blinkt nur solange die Warnung am Cube noch nicht angetippt wurde, stoppt sofort wenn der Nutzer bestätigt
- `dwdWarningSeen`-Variable entfernt (redundant, durch `dwdWarnBestaetigt` ersetzt)

---

## v0.5.0-beta (2026-06-27)

### Neu
- **Hardware-Addon API `/api/ampel`:** Neuer JSON-Endpunkt für ESP32-C3 Ampel-Addon – liefert aktuelle Temperatur, berechnete Ampelfarbe (green/yellow/red), DWD-Warnstatus und alle 6 Temperaturschwellen
- **`dwd_warning`** im API-Response ist `true` nur solange neue Warnungen noch nicht durch Öffnen der Warnkarte im Cube bestätigt wurden
- **Ampel-Konfiguration in WebUI:** Neue Sektion mit 6 Zahlenfeldern (min/max für grün/gelb/rot) + Live-Anzeige der aktuellen Temperatur und aktiven Farbe
- Schwellwerte werden in Preferences gespeichert (Defaults: grün 15–19°C, gelb 20–24°C, rot 25–99°C)

---

## v0.4.1-beta (2026-06-27)

### Neu
- **DWD-Warn-Indikator auf Hauptscreen:** Roter blinkender Button in der Nav-Reihe (Mitte unten) erscheint sobald aktive DWD-Warnungen vorliegen – Antippen stoppt das Blinken und öffnet direkt die DWD-Warnkarte, Button bleibt dauerhaft rot solange Warnungen aktiv sind

---

## v0.4.0-beta (2026-06-27)

### Neu
- **DWD Warnkarte (ScreenWarnkarte2):** Statt WMS-Karte jetzt farbige Warnkarten-Liste direkt aus dem DWD GeoServer WFS – bis zu 5 aktive Warnungen standortbezogen, farbkodiert nach DWD-Warnstufe (gelb/orange/rot/violett)
- Warnkarten sind tippbar: Detail-Popup mit vollständiger Warnmeldung
- Warn-Screens (Regen & Pollen) zeigen beim Antippen ein Popup mit Warndetails

### Fixes
- Umlaute (ä/ö/ü/ß) in DWD-Warnmeldungen und Pollennamen korrekt dargestellt (ä→ae usw.)
- OK-Button im Warn-Popup überdeckt nicht mehr den Beschreibungstext (Popup-Höhe 270 px)
- DWD Wetterwarnungen: API-Abruf über WFS BBOX-Filter statt nicht mehr erreichbarer `/warnings.json`-URL

---

## v0.3.3-beta (2026-06-25)

### Fixes
- NTP-Zeitsynchronisierung deutlich schneller: 3 Server parallel (`pool.ntp.org`, `time.cloudflare.com`, `time.google.com`) statt nur einem
- Pollenwarnung: `pollenWarnGezeigt` und `pollenWarnBestaetigt` werden nach jedem stündlichen Update zurückgesetzt – Warnung erscheint bei erneuter Belastung wieder

---

## v0.3.2-beta (2026-06-21)

### Fixes
- Windpfeil auf Radarkarte: Hintergrundkreis jetzt halbtransparent (Opacity 90/255 statt 160/255)

---

## v0.3.1-beta (2026-06-15)

### Neu
- Standort-Marker auf der Radarkarte: roter Punkt zeigt den konfigurierten Standort im Bundesland-Ausschnitt

---

## v0.3.0-beta (2026-06-15)

### Neu
- Neuer Screen **Sonnenaufgang/Untergang**: UV-Index (Tages-Maximum), Sonnenaufgangszeit, Sonnenuntergangszeit
- Navigation erweitert: screen_1 → Wetter → Pollen → Radar → **Sonnenaufgang/untergang** → screen_1
- Zwei neue Icons auf SunMoon-Screen: Sonne (`day_clear`) und Mond (`night_full_moon_clear`)

---

## v0.2.16-beta (2026-06-14)

### Neu
- Nachtmodus: zeitbasiertes Display-Dimmen (Von/Bis in 15-Min-Schritten, eigene Nacht-Helligkeit 0–50 %)
- Nachtmodus per WebUI konfigurierbar unter Einstellungen → Display
- Pollen-Schwellwerte an WetterCube angeglichen: ≤10→gering, ≤30→mittel, ≤100→hoch, >100→stark (Grains/m³)
- Pollenwarnung: nächste Stunde korrekt (min(aktuelle Stunde +1, 23))

### Fixes
- Nachtmodus: Display blinkt nicht mehr beim Touch (Helligkeit im Nachtmodus nur über Loop geregelt, kein Reset durch Touch)
- Radarkarte: Automatischer Fallback auf nur `dwd:Niederschlagsradar` wenn `KV_VG250_BUNDESLAENDER_2020`-Layer DWD-seitig nicht erreichbar (internalError)
- Radarkarte: Content-Type-Prüfung vor PNG-Dekodierung verhindert lodepng-Fehler bei WMS-XML-Fehlerantwort

---

## v0.2.15-beta (2026-06-14)

### Neu
- Eigene Partitionstabelle (`partitions.csv`): 2× 4 MB OTA-Slots statt 2× 1,5 MB – Speicherauslastung sinkt von 88 % auf ~66 %
- SPIFFS bleibt mit ~8 MB erhalten (vorher 9,9 MB – kein Funktionsverlust, SPIFFS wird nicht genutzt)
- ⚠️ Einmalige USB-Flashpflicht um Partitionstabelle einzubrennen, danach OTA wieder normal

---

## v0.2.14-beta (2026-06-14)

### Fixes
- Alle Farben korrekt: `build_opt.h` mit `LV_COLOR_16_SWAP=1` hinzugefügt — LVGL tauscht Bytes vor der Ausgabe, `swap565_t` in `disp_flush` arbeitet korrekt
- Forecast-Temperaturen: Farbkodierung per `tempColor()` (blau/grün/gelb/rot) wie auf dem Hauptscreen

---

## v0.2.12-beta (2026-06-14)

### Fixes
- Radarkarte: Download-Methode auf `http.getString()` umgestellt – verhindert truncated PNG bei Chunked-Transfer ohne Content-Length-Header
- Radarkarte: Aktualisierungsintervall von 30 auf 10 Minuten reduziert (DWD Radar aktualisiert alle 5 Min.)
- Radarkarte: Bild-Alignment auf `LV_ALIGN_TOP_MID`, Höhe auf 270px reduziert – Navigationsbuttons (Y=273) werden nicht mehr überdeckt
- Radarkarte: Layer-Kombination `dwd:KV_VG250_BUNDESLAENDER_2020` + `dwd:Niederschlagsradar` für Bundesland-Konturen + Niederschlag

---

## v0.2.11-beta (2026-06-14)

### Neu
- Warnkarte ersetzt durch DWD WMS Niederschlagsradar (`niederschlagsradar`-Layer)
- Radarkarte ist bundeslandbezogen: zeigt nur den Ausschnitt des im Setup gewählten Bundeslandes
- Keine eigene Bundesland-Auswahl nötig — nutzt vorhandene `dwd_region`-Einstellung
- Neue `DwdBbox`-Tabelle mit EPSG:4326-Koordinaten für alle 15 DWD-Regionen
- WMS-Anfrage: 480×280 px, passt exakt ins Display (40px für Navigation)
- Aktualisierung alle 30 Minuten wie bisher

---

## v0.2.10-beta (2026-06-13)

### Änderungen
- Screen 1 Top-3 Pollen: verwendet jetzt Open-Meteo Stundenwerte (nächste Stunde) statt DWD Tageswerte
- Pollenwarnung: löst jetzt basierend auf Open-Meteo Stundenwerten (nächste Stunde) aus statt DWD Tageswerten
- DWD Pollenwerte bleiben unverändert auf dem 3-Tage-Pollenscreen
- Neue Hilfsfunktion `openMeteoToDwd()`: rechnet Körner/m³ auf DWD 0–3 Skala um (<25→gering, <75→mittel, <150→hoch, ≥150→stark)

---

## v0.2.9.1-beta (2026-06-13)

### Fixes
- DWD Pollenflug: JSON-Buffer von 8192 auf 16384 Bytes erhöht – verhindert Parsing-Fehler bei großer API-Antwort
- DWD Pollenflug: Debug-Logging im Serial Monitor (`[Pollen]`)

---

## v0.2.9-beta (2026-06-13)

### Fixes
- Temperatur-Farbcodierung auf WetterCube-Schwellen angeglichen: blau < 8°C · grün 8–15°C · gelb 15–24°C · rot ≥ 24°C

---

## v0.2.8-beta (2026-06-13)

### Neu
- Wetter-Icons: overcast und thunder hinzugefügt (via PicoPixel als 256×256 RGB565A8)
- `wmoZuImage`: WMO-Codes jetzt mit korrekten Icons (Bewölkung → overcast, Gewitter → thunder)
- Lagerscreen in PicoPixel als unsichtbarer Asset-Container für Icons

---

## v0.2.7-beta (2026-06-13)

### Fixes
- Boot-Screen jetzt sichtbar: `lv_scr_load(objects.screenboot)` explizit nach `ui_init()` – PicoPixel lädt `screen_1` als Standard, nicht `screenboot`
- Boot-Screen bleibt bis alle Daten geladen sind: `aktualisiereUI()` vor `loadScreen()`
- Boot-Screen Rendering: `lvgl_flush()` (80ms Loop) statt einmaligem `lv_timer_handler()` – Display wird vollständig gezeichnet
- Ellipsis-Zeichen `…` in Boot-Meldungen durch `...` ersetzt – war als Kästchen dargestellt (Zeichen nicht in PicoPixel-Font enthalten)
- Web-Installer Versionsnummer aktualisiert

---

## v0.2.4-beta (2026-06-13)

### Fixes
- OTA: Firmware-Download jetzt über GitHub Pages (`jppeterson-lab.github.io`) statt GitHub Releases – keine Redirects, zuverlässige HTTPS-Verbindung vom ESP32
- OTA: `server.send()` erst nach erfolgreichem Download (wie WetterCube) – verhindert Netzwerkkonflikt durch offene WebServer-Verbindung
- OTA: `httpUpdate`-Library komplett entfernt, ersetzt durch direktes `HTTPClient` + `Update.writeStream()`

---

## v0.2.3-beta (2026-06-13)

### Intern
- Zwischenversion für OTA-Tests (selbe Firmware wie v0.2.2-beta)

---

## v0.2.2-beta (2026-06-13)

### Fixes
- OTA: Erster Versuch manueller CDN-Redirect-Auflösung (nicht erfolgreich – github.com selbst war vom ESP32 nicht erreichbar)

---

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

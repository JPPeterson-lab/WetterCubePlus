# Changelog

*[Deutsche Version](CHANGELOG.md)*

## v0.9.8-rc4 (2026-09-18)

### New
- **`/log` now also shows the last `/api/ampel` request:** new line "Last /api/ampel request: Xs ago" in the diagnostic log – the WetterAmpel polls every 30s, so a much higher value directly proves that the cube was temporarily unreachable for the traffic light (instead of just guessing)

---

## v0.9.8-rc3 (2026-09-11)

### New
- **Remote diagnostic log via browser:** New `/log` endpoint shows the last 25 diagnostic events (RAM ring buffer), free heap and uptime – no USB/Serial on site needed, accessible directly via the OTA-capable WebUI
- **fetchWetter() now logs errors:** HTTP and JSON parse errors now land in the `/log` buffer (incl. free heap at the time of the error), instead of vanishing completely silently as before
- **`/api/ampel` now returns `data_age_min`:** shows how many minutes have passed since the last successful weather data update – helps diagnose a "stuck" traffic light

### WetterAmpel add-on (ESP32-C3)
- Reused `WiFiClient` instead of recreating one per request (against heap fragmentation over many hours of runtime)
- Preventive 24h self-restart
- Additional heap logging on errors

---

## v0.9.8-rc1 (2026-08-09)

### Fixes
- **Unified AQI/PM2.5/ozone colors between ScreenAirQuality and ScreenHealth:** both screens sometimes showed different colors for the same pollutant – caused by a duplicated, diverging PM2.5 color scale as well as a time-period mismatch (current hour vs. 3h forecast-max). ScreenHealth now uses the same current values and the same color functions as ScreenAirQuality – an identical value is now guaranteed to produce an identical color
- **Biowetter period now visible on ScreenHealth:** now shows "This afternoon" / "Tomorrow morning" instead of a generic title, so the relation to the period on screenbiowetter is clearly recognizable

---

## v0.9.7-rc4 (2026-08-09)

### New
- **Bubblebreaker: Undo** – one step can be undone, button in the game HUD (visually distinguished active/inactive)
- **Bubblebreaker: Game modes** – selection on the start screen between "Normal" (as before) and "Endless" (columns that become empty are automatically refilled with new bubbles, the game continues until no combination is left)

### Changed
- **Bubblebreaker color:** orange replaced with lilac – better distinguishable from red and yellow

---

## v0.9.7-rc3 (2026-08-09)

### Fixes
- **UI corrections:** Minor layout errors fixed (PicoPixel re-export)

---

## v0.9.7-rc2 (2026-08-09)

### Changed
- **Breakout mini-game removed, replaced with Bubblebreaker** – Breakout hit hard memory limits of the ESP32-S3 (internal DRAM is shared between LVGL and WiFi/TLS); Bubblebreaker is implemented more memory-efficiently: the playing field is a single canvas with a pixel buffer in PSRAM instead of many individual objects. Tap and clear colorful, connected color groups, selection screen with a persistent high-score list (top 5), started from the menu screen, exit button reachable at all times
- **Dark mode completed** – full dark color theme for all screens, backgrounds and controls (previously an experimental skeleton), toggle in the menu screen

---

## v0.9.7-rc1 (2026-08-08)

### New
- **Mini-game: Breakout** – new menu item starts a Breakout game directly on the cube: white background with colorful bricks, 5 selectable difficulty levels (beginner to extreme), controlled via arrow buttons (doesn't cover the paddle like touch-drag would), exit button reachable at all times back to the main screen, persistent high-score list (top 5, survives restart)

---

## v0.9.6-rc2 (2026-08-02)

### Fixes
- **UI corrections:** Layout errors fixed on several screens (PicoPixel re-export)

---

## v0.9.6-rc1 (2026-08-01)

### Changes
- **Air quality screen:** ArcAQI stays color-coded; `labelaqivalue` and `labelaqistatus` use the theme's text color
- **Bars + numeric values color-coded:** PM2.5, PM10, NO₂ and ozone – both bars and numeric labels are now colored according to EU limit values

---

## v0.9.6-rc (2026-08-01)

### Status
- **Release candidate:** feature set complete, final UI corrections and stability tests

### Changes
- UI revisions and layout corrections (PicoPixel re-export)

---

## v0.9.5.2-beta (2026-08-01)

### Fixes
- **UI corrections:** Further layout errors fixed (PicoPixel re-export)

---

## v0.9.5.1-beta (2026-08-01)

### Fixes
- **UI corrections:** Various layout errors fixed on several screens (PicoPixel export)

---

## v0.9.5-beta (2026-08-01)

### New
- **Icon navigation:** Arrow icons (`fc_left_60`, `fc_right_60`) replace text labels as navigation buttons; additional icons `fc_right_up2_38` / `fc_left_down2_38` prepared for sub-screen navigation
- **Biowetter categories unified:** screenhealth and WebUI now use exactly the same DWD labels as screenbiowetter (general condition, hypotension, high blood pressure, inflammatory rheumatism, degenerative rheumatism, asthma, heat strain)

### Fixes
- **Icon buttons not clickable:** `labelbuttonforward` / `labelbuttonbackward` could no longer be triggered by touch after converting to `lv_img`; fix: `LV_OBJ_FLAG_CLICKABLE` set in `registriereCallbacks()`
- **Pollen screen_1:** third pollen label removed (only 2 slots remain), code adjusted accordingly

---

## v0.9.4-beta (2026-07-26)

### New
- **Firmware version on menu screen:** `labelversion` shows the current firmware version
- **Biowetter period time-dependent:** Screenhealth shows "this afternoon" before 6pm, "tomorrow morning" from 6pm onward – so the next relevant period is always shown (instead of always period 0)

### Fixes
- **UV/light category showed the afternoon forecast in the morning:** Due to a fixed period 0 (this afternoon), UV exposure could appear "high" even though the current UV index was low; fix: time-based period selection

---

## v0.9.3-beta (2026-07-25)

### New
- **Health screen footer: next-3-hours preview** – AQI, PM2.5, ozone and UV index now show the **max value of the next 3 hours** instead of current/daily values; UV index is now loaded hourly from Open-Meteo (previously the daily maximum)
- **Biowetter zone: plain-text label** – dropdown now shows e.g. "A – Schleswig-Holstein, Hamburg, northern Lower Saxony, Bremen" instead of "Zone A"; zone K (Swabia, Upper Bavaria) added (was previously missing)

### Fixes
- **O3 dot showed the PM2.5 value** – `labelwso3` incorrectly used `pollen.pm25_next`; fix: `pollen.o3_3h` (ozone max 3h)

---

## v0.9.2-beta (2026-07-25)

### Fixes
- **Theme switch destroyed color coding:** `change_color_theme()` in `actions.c` overwrites all text including pollen- and biowetter-color-coded labels on screenhealth. Fix: a second event handler on `labelswitchtheme` calls `aktualisiereUI()` after the theme switch – colors are reapplied immediately
- **Traffic-light dots instead of numbers:** AQI, PM2.5, next-hour PM2.5 and UV index in the health screen footer are now shown as colored circles (20×20 px) instead of numeric values – saves space and conveys status at a glance
- **Clock on screenhealth freezes:** `labelwstime` and `labeldatum_1` were only set during the 10-minute data fetch (`aktualisiereUI()`), not in the 60-second loop. Fix: both labels added to the per-minute timer block

---

## v0.9.0-beta (2026-07-25)

### New
- **Health screen (screenhealth):** Compact overview for weather-sensitive people – top 4 pollen by current DWD daily value (sorted), 4 configurable biowetter categories (period: this afternoon), AQI and UV index in the footer
- **Weather sensitivity configurable:** 4 categories for the health screen selectable via WebUI (cardiovascular, respiratory, rheumatism, migraine, mental health, cold risk, UV/light)
- **Biowetter zone in WebUI:** DWD zone (A–J) can now be set directly in the web interface (previously only changeable in code)
- **fc_plus button on screen 1:** New icon opens the health screen directly
- **Health screen navigation:** `<` → Biowetter, `>` → weather forecast, home icon → screen 1, menu icon → menu screen

### Fixes
- **WebUI settings visible immediately:** After saving in the WebUI, `aktualisiereUI()` is called – changes (e.g. biowetter categories) appear immediately on the display
- **AQI on health screen:** `labelwsaqi` showed the PM2.5 value instead of the European AQI; fix: use `pollen.aqi`

---

## v0.8.1-beta (2026-07-19)

### Fixes
- **Biowetter: empty periods shown as `–`** – When DWD only returns "no impact" for a period (forecast not yet published), a dark `–` is shown instead of seven grey "none" labels
- **Biowetter: JSON buffer increased** – `DynamicJsonDocument` raised from 16 KB to 32 KB (11 zones buffered more safely)
- **Biowetter: debug output for all periods** – Serial now shows entries and raw values per period for diagnostics

---

## v0.8.0-beta (2026-07-19)

### New
- **DWD Biowetter (2 screens):** Health-related weather effects for 7 categories (general condition, low/high blood pressure, inflammatory/degenerative rheumatism, asthma, heat strain) – directly from `opendata.dwd.de/climate_environment/health/alerts/biowetter.json`, zone A–J configurable
- **Biowetter navigation:** screenairquality `>` → screenbiowetter → `M` button for screenbiowetter2 (tomorrow/day after); back via `<` → screenairquality
- **Menu icon on screen 1:** `fc_settings` image opens the menu screen (replaces labelbuttonmenu)

### Fixes
- **HTTPS stream abort (IncompleteInput):** the 110 KB `biowetter.json` was streamed directly into ArduinoJson via `getStream()` and aborted; fix: buffer via `getString()` into PSRAM first, then parse
- **fc_settings not clickable:** `lv_img_create()` is not clickable by default; fix: `LV_OBJ_FLAG_CLICKABLE` set in `registriereCallbacks()`
- **Biowetter JSON buffer:** `DynamicJsonDocument` raised from 8 KB to 16 KB, filter doc from 512 to 1024 bytes

---

## v0.7.0-beta (2026-07-18)

### New
- **Air quality screen (AQI):** New screen with European AQI as an arc gauge (0–150, color-coded), individual values PM2.5, PM10, NO₂, ozone with bars and EU-limit color coding
- **AQI in navigation:** screen inserted between sun & moon and the main screen (screensunmoon → screenairquality → screen_1)
- **Open-Meteo Air Quality API** now also delivers PM2.5, PM10, NO₂ and ozone (same endpoint as pollen, no second HTTP call)

### Fixes
- **LVGL heap increased:** `LV_MEM_SIZE` raised from 64 KB to 128 KB (out-of-memory when loading the menu screen with the new AQI screen)
- **Font fix:** `mg/m²` (U+00B2) replaced with `ug/m3` – the built-in Montserrat font has no ² character; deploy_ui.sh now patches this automatically going forward
- **Navigation wiring:** After the UI re-export all button numbers had shifted; the REG_CB block was fully remapped

---

## v0.6.1-beta (2026-07-15)

### New
- **4 new weather icons:** `day_partial_cloud`, `night_full_moon_partial_cloud`, `sleet`, `fog`
- **Day/night icons:** main-screen icon now automatically switches between day and night variants (`is_day` from Open-Meteo)
- **Refined WMO mapping:** fog (WMO 45–48) and sleet (WMO 58–67) now have dedicated icons instead of falling back to `rain`/`overcast`

---

## v0.6.0-beta (2026-07-12)

### New
- **Menu screen: brightness slider** – arc slider directly on the device, change takes effect immediately and is saved
- **Menu screen: rain warning on/off** – toggle switch, state is persisted
- **Menu screen: pollen warning on/off** – toggle switch, state is persisted
- **Menu screen: DWD warnings on/off** – toggle switch; disabling it immediately removes the warning indicator on screen_1

---

## v0.5.4-beta (2026-07-12)

### New
- **Menu screen (ScreenMenu):** New screen reachable via `LabelButtonMenu` on screen_1 – `LabelButtonMenu_2` leads back to screen_1
- **Theme switch (experimental):** `LabelSwitchTheme` in the menu toggles between the "Light" and "Summer" themes – groundwork for the later full theme system, still under development at this point

---

## v0.5.3-beta (2026-07-11)

### New
- **Home button on ScreenSunMoon:** tapping returns to screen 1 (previously not wired up)
- **Home button on ScreenForecastPollenHour:** tapping returns to screen 1 (previously not wired up)
- **Daily min/max temperature on ScreenSunMoon:** daily low and high temperature (`LabelTempminValue` / `LabelTempmaxValue`) with temperature-based color coding (blue/green/yellow/red)

---

## v0.5.2-beta (2026-06-29)

### Fixes
- **Traffic-light API `active` calculation:** cascaded comparison over `min` values instead of min+max ranges — no more gaps between thresholds, the traffic light always shows a color as long as the temperature is ≥ green_min

---

## v0.5.1-beta (2026-06-28)

### Fixes
- **Hourly pollen forecast:** slots no longer clamp at 23h — the API index now runs across the day boundary (0–47), displayed hour correctly computed with `% 24`
- **Traffic-light add-on DWD logic:** `dwd_warning` in the API response now exactly mirrors the blink state of the screen_1 warning button — only blinks as long as the warning hasn't been tapped on the cube yet, stops immediately once the user confirms it
- `dwdWarningSeen` variable removed (redundant, replaced by `dwdWarnBestaetigt`)

---

## v0.5.0-beta (2026-06-27)

### New
- **Hardware add-on API `/api/ampel`:** New JSON endpoint for the ESP32-C3 traffic-light add-on – returns current temperature, computed traffic-light color (green/yellow/red), DWD warning status and all 6 temperature thresholds
- **`dwd_warning`** in the API response is `true` only as long as new warnings haven't yet been confirmed by opening the warning map on the cube
- **Traffic-light configuration in WebUI:** New section with 6 numeric fields (min/max for green/yellow/red) + live display of the current temperature and active color
- Thresholds are stored in Preferences (defaults: green 15–19°C, yellow 20–24°C, red 25–99°C)

---

## v0.4.1-beta (2026-06-27)

### New
- **DWD warning indicator on the main screen:** Red blinking button in the nav row (bottom center) appears as soon as active DWD warnings exist – tapping stops the blinking and opens the DWD warning map directly, button stays red for as long as warnings are active

---

## v0.4.0-beta (2026-06-27)

### New
- **DWD warning map (ScreenWarnkarte2):** Instead of a WMS map, now a colored warning-list directly from the DWD GeoServer WFS – up to 5 active warnings, location-based, color-coded by DWD warning level (yellow/orange/red/violet)
- Warning cards are tappable: detail popup with the full warning message
- Warning screens (rain & pollen) show a popup with warning details when tapped

### Fixes
- Umlauts (ä/ö/ü/ß) in DWD warning messages and pollen names now rendered correctly (ä→ae, etc.)
- OK button in the warning popup no longer covers the description text (popup height 270 px)
- DWD weather warnings: API fetch now via WFS BBOX filter instead of the no-longer-reachable `/warnings.json` URL

---

## v0.3.3-beta (2026-06-25)

### Fixes
- NTP time sync significantly faster: 3 servers in parallel (`pool.ntp.org`, `time.cloudflare.com`, `time.google.com`) instead of just one
- Pollen warning: `pollenWarnGezeigt` and `pollenWarnBestaetigt` are reset after every hourly update – the warning reappears if exposure rises again

---

## v0.3.2-beta (2026-06-21)

### Fixes
- Wind-arrow on the radar map: background circle now semi-transparent (opacity 90/255 instead of 160/255)

---

## v0.3.1-beta (2026-06-15)

### New
- Location marker on the radar map: red dot shows the configured location within the state's map section

---

## v0.3.0-beta (2026-06-15)

### New
- New screen **sunrise/sunset**: UV index (daily maximum), sunrise time, sunset time
- Navigation extended: screen_1 → weather → pollen → radar → **sunrise/sunset** → screen_1
- Two new icons on the SunMoon screen: sun (`day_clear`) and moon (`night_full_moon_clear`)

---

## v0.2.16-beta (2026-06-14)

### New
- Night mode: time-based display dimming (from/to in 15-min steps, separate night brightness 0–50%)
- Night mode configurable via WebUI under Settings → Display
- Pollen thresholds aligned with WetterCube: ≤10→low, ≤30→medium, ≤100→high, >100→very high (grains/m³)
- Pollen warning: next hour now correct (min(current hour +1, 23))

### Fixes
- Night mode: display no longer flickers on touch (brightness in night mode only controlled via the loop, no reset triggered by touch)
- Radar map: automatic fallback to just `dwd:Niederschlagsradar` when the `KV_VG250_BUNDESLAENDER_2020` layer is unreachable on the DWD side (internalError)
- Radar map: content-type check before PNG decoding prevents lodepng errors on a WMS XML error response

---

## v0.2.15-beta (2026-06-14)

### New
- Custom partition table (`partitions.csv`): 2× 4 MB OTA slots instead of 2× 1.5 MB – storage usage drops from 88% to ~66%
- SPIFFS remains at ~8 MB (previously 9.9 MB – no functional loss, SPIFFS is unused)
- ⚠️ One-time mandatory USB flash to burn in the new partition table, OTA works normally again afterwards

---

## v0.2.14-beta (2026-06-14)

### Fixes
- All colors correct: `build_opt.h` with `LV_COLOR_16_SWAP=1` added — LVGL swaps bytes before output, `swap565_t` in `disp_flush` now works correctly
- Forecast temperatures: color-coded via `tempColor()` (blue/green/yellow/red) same as on the main screen

---

## v0.2.12-beta (2026-06-14)

### Fixes
- Radar map: download method switched to `http.getString()` – prevents truncated PNG with chunked transfer and no Content-Length header
- Radar map: refresh interval reduced from 30 to 10 minutes (DWD radar refreshes every 5 min)
- Radar map: image alignment set to `LV_ALIGN_TOP_MID`, height reduced to 270px – navigation buttons (Y=273) are no longer covered
- Radar map: layer combination `dwd:KV_VG250_BUNDESLAENDER_2020` + `dwd:Niederschlagsradar` for state outlines + precipitation

---

## v0.2.11-beta (2026-06-14)

### New
- Warning map replaced with the DWD WMS precipitation radar (`niederschlagsradar` layer)
- Radar map is state-specific: shows only the section for the state chosen in setup
- No separate state selection needed — reuses the existing `dwd_region` setting
- New `DwdBbox` table with EPSG:4326 coordinates for all 15 DWD regions
- WMS request: 480×280 px, fits the display exactly (40px reserved for navigation)
- Refreshes every 30 minutes as before

---

## v0.2.10-beta (2026-06-13)

### Changes
- Screen 1 top-3 pollen: now uses Open-Meteo hourly values (next hour) instead of DWD daily values
- Pollen warning: now triggers based on Open-Meteo hourly values (next hour) instead of DWD daily values
- DWD pollen values remain unchanged on the 3-day pollen screen
- New helper function `openMeteoToDwd()`: converts grains/m³ to the DWD 0–3 scale (<25→low, <75→medium, <150→high, ≥150→very high)

---

## v0.2.9.1-beta (2026-06-13)

### Fixes
- DWD pollen flight: JSON buffer raised from 8192 to 16384 bytes – prevents parsing errors on large API responses
- DWD pollen flight: debug logging in the Serial monitor (`[Pollen]`)

---

## v0.2.9-beta (2026-06-13)

### Fixes
- Temperature color coding aligned with WetterCube thresholds: blue < 8°C · green 8–15°C · yellow 15–24°C · red ≥ 24°C

---

## v0.2.8-beta (2026-06-13)

### New
- Weather icons: overcast and thunder added (via PicoPixel as 256×256 RGB565A8)
- `wmoZuImage`: WMO codes now use the correct icons (cloudy → overcast, thunderstorm → thunder)
- Storage screen in PicoPixel as an invisible asset container for icons

---

## v0.2.7-beta (2026-06-13)

### Fixes
- Boot screen now visible: `lv_scr_load(objects.screenboot)` explicitly called after `ui_init()` – PicoPixel loads `screen_1` by default, not `screenboot`
- Boot screen stays until all data is loaded: `aktualisiereUI()` before `loadScreen()`
- Boot screen rendering: `lvgl_flush()` (80ms loop) instead of a single `lv_timer_handler()` call – display is fully drawn
- Ellipsis character `…` in boot messages replaced with `...` – was rendered as a box (character not included in the PicoPixel font)
- Web installer version number updated

---

## v0.2.4-beta (2026-06-13)

### Fixes
- OTA: firmware download now via GitHub Pages (`jppeterson-lab.github.io`) instead of GitHub Releases – no redirects, reliable HTTPS connection from the ESP32
- OTA: `server.send()` only called after a successful download (like WetterCube) – prevents a network conflict from an open WebServer connection
- OTA: `httpUpdate` library removed entirely, replaced with direct `HTTPClient` + `Update.writeStream()`

---

## v0.2.3-beta (2026-06-13)

### Internal
- Intermediate version for OTA tests (same firmware as v0.2.2-beta)

---

## v0.2.2-beta (2026-06-13)

### Fixes
- OTA: first attempt at manual CDN redirect resolution (unsuccessful – github.com itself was unreachable from the ESP32)

---

## v0.2.1-beta (2026-06-13)

### New
- Home button on all screens: returns directly to screen 1
- White UI theme implemented in PicoPixel (reduces reflections on the glass cover)

### Fixes
- OTA update: fixed wrong filename in the download path (`firmware.bin` instead of `WetterCubePlus-X.X.X.bin`)
- OTA update: `HTTPC_FORCE_FOLLOW_REDIRECTS` for GitHub CDN redirects

---

## v0.2.0-beta (2026-06-13)

### New
- DWD warning map: Germany warning map as its own screen (PNG decoding via lodepng in PSRAM)
- Warning-screen background blinks instead of the icon/text (better visibility)
- deploy_ui.sh: automatic fix for a PicoPixel ARGB color bug (`lv_color_hex(0xffRRGGBB)` → `0xRRGGBB`)
- Light theme: white background optionally configurable (reduces reflections on glass)

### Fixes
- **Brightness**: backlight PWM now driven directly via the ESP-IDF LEDC API (no Arduino wrapper) – reliable on all GPIOs
- **Warning map**: lodepng now uses PSRAM allocation (`heap_caps_malloc`) instead of the LVGL heap (was 128 KB, too small for PNG decoding)
- **Warning-map download**: stream loop replaced with a robust `readBytes` loop – prevents aborts with HTTPS/chunked encoding
- **Rain warning**: now correctly checks the current + next hour (was off-by-one)
- **Pollen warning**: threshold comparison fixed (`>` instead of `>=`) – "high" no longer warns right at the "high" threshold
- **Colors**: `LV_COLOR_16_SWAP 1` restored (had accidentally been set to 0 → wrong colors)
- **Screen black background looked bluish**: PicoPixel exports `0xff000000` as ARGB, LVGL interpreted `0xFF` as the red channel

### Internal
- Light_PWM removed from the LovyanGFX class; backlight now fully manually controlled via LEDC
- LVGL screen transition: FADE_IN replaced with NONE (crash fix on rapid screen changes)

---

## v0.1.1-beta (2026-06-12)

### New
- PicoPixel UI fully integrated: boot screen, main screen, weather forecast, pollen forecast, warning-map screen, rain warning, pollen warning
- All weather icons embedded as RGB565A8 (PSRAM, LV_COLOR_16_SWAP-compatible)
- Touch XPT2046 on its own SPI bus (SPI3_HOST), no collision with the display

### Fixes
- LVGL FADE_IN crash fixed
- LV_COLOR_16_SWAP + swap565_t configured correctly

---

## v0.1.0-beta (2026-06-11)

- First release
- Base structure: LovyanGFX + LVGL 8, PSRAM buffer, WebUI, captive portal, OTA
- Data sources: Open-Meteo, DWD pollen flight, DWD warnings
- Web installer via GitHub Pages

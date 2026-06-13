// ============================================================
//  WetterCubePlus.ino
//  ESP32-S3 N16R8 | ILI9488 3.5" 480x320 | XPT2046 Touch
//  LVGL 8.x | LovyanGFX | HTTP-OTA | WebUI | DWD-Warnungen
//  Version: 0.1.1-beta
// ============================================================

#include "webui_html.h"

// ---- Versions-Define (muss mit docs/version.json übereinstimmen!) ----
#define FIRMWARE_VERSION "0.2.8-beta"
#define OTA_VERSION_URL  "https://raw.githubusercontent.com/JPPeterson-lab/WetterCubePlus/main/docs/version.json"
#define OTA_BIN_URL      "https://jppeterson-lab.github.io/WetterCubePlus/firmware/firmware.bin"
#define MDNS_NAME        "wettercubeplus"

// ============================================================
//  Pin-Definitionen
// ============================================================
// -- Display SPI (SPI2_HOST) --
#define TFT_SCLK  14
#define TFT_MOSI  13
#define TFT_MISO  12
#define TFT_DC     2
#define TFT_CS    15
#define TFT_RST   16
#define TFT_BL    17

// -- Touch XPT2046 (EIGENER SPI-Bus – separate Pins auf dem Modul!) --
#define TOUCH_CS   21
#define TOUCH_IRQ  18
#define TOUCH_CLK   6
#define TOUCH_MOSI  7
#define TOUCH_MISO  8

// -- Display-Auflösung --
#define TFT_WIDTH  480
#define TFT_HEIGHT 320

// ============================================================
//  Includes
// ============================================================
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "src/ui/ui.h"
#include "src/ui/images.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
extern "C" {
  unsigned lodepng_decode32(unsigned char** out, unsigned* w, unsigned* h,
                             const unsigned char* in, size_t insize);
}

// ============================================================
//  LovyanGFX – ILI9488 + XPT2046
// ============================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488  _panel;
  lgfx::Bus_SPI        _bus;
  lgfx::Touch_XPT2046  _touch;
public:
  LGFX() {
    { auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.pin_sclk    = TFT_SCLK;
      cfg.pin_mosi    = TFT_MOSI;
      cfg.pin_miso    = TFT_MISO;
      cfg.pin_dc      = TFT_DC;
      cfg.freq_write  = 40000000;
      cfg.freq_read   =  8000000;
      _bus.config(cfg);
      _panel.setBus(&_bus); }

    { auto cfg = _panel.config();
      cfg.pin_cs    = TFT_CS;
      cfg.pin_rst   = TFT_RST;
      cfg.panel_width   = 320;
      cfg.panel_height  = 480;
      cfg.memory_width  = 320;
      cfg.memory_height = 480;
      cfg.rgb_order = false;  // ILI9488 = BGR; wenn Farben invertiert → true
      cfg.invert    = false;
      _panel.config(cfg); }

    { auto cfg = _touch.config();
      cfg.spi_host   = SPI3_HOST;   // eigener SPI-Host für Touch
      cfg.pin_sclk   = TOUCH_CLK;
      cfg.pin_mosi   = TOUCH_MOSI;
      cfg.pin_miso   = TOUCH_MISO;
      cfg.pin_cs     = TOUCH_CS;
      cfg.pin_int    = -1;          // Polling
      cfg.freq       = 1000000;
      cfg.x_min      = 300;
      cfg.x_max      = 3800;
      cfg.y_min      = 300;
      cfg.y_max      = 3800;
      cfg.bus_shared = false;
      _touch.config(cfg);
      _panel.setTouch(&_touch); }

    setPanel(&_panel);
  }
};

LGFX tft;

// ============================================================
//  Globale Objekte
// ============================================================
Preferences  prefs;
WebServer    server(80);
DNSServer    dnsServer;

// ============================================================
//  Konfiguration (aus Preferences geladen)
// ============================================================
struct Config {
  String ssid;
  String pass;
  String location;
  float  lat = 0.0f;
  float  lon = 0.0f;
  int    dwd_region = 31;       // 31 = Berlin; vollständige Liste siehe unten
  bool   regen_warn = true;
  bool   pollen_warn = true;
  int    pollen_schwelle = 2;   // 0=gering 1=mittel 2=hoch 3=stark
  int    dim_timeout = 3;       // Minuten; 0 = aus
  int    brightness = 80;       // Prozent
  bool   warn_region = true;    // DWD-Warnungen aktiv
};
Config cfg;

// ============================================================
//  DWD-Regionen (region_id → Bezeichnung)
// ============================================================
struct DwdRegion { int id; const char* name; };
const DwdRegion DWD_REGIONS[] = {
  {10, "Schleswig-Holstein und Hamburg"},
  {20, "Mecklenburg-Vorpommern"},
  {30, "Niedersachsen und Bremen"},
  {31, "Brandenburg und Berlin"},
  {32, "Sachsen-Anhalt"},
  {33, "Thüringen"},
  {34, "Sachsen"},
  {40, "Nordrhein-Westfalen"},
  {50, "Hessen"},
  {60, "Rheinland-Pfalz und Saarland"},
  {71, "Bayern – Alpen"},
  {72, "Bayern – Alpenvorland"},
  {73, "Bayern – Ost"},
  {74, "Bayern – Nord"},
  {80, "Baden-Württemberg"},
  {0,  nullptr}
};

// ============================================================
//  Daten-Structs
// ============================================================
struct WetterDaten {
  float  temp        = 0.0f;
  float  feels_like  = 0.0f;
  float  humidity    = 0.0f;
  float  wind_speed  = 0.0f;
  int    wind_dir    = 0;
  float  pressure    = 0.0f;
  int    wmo_code    = 0;
  float  uv_index    = 0.0f;
  String sunrise;
  String sunset;
  float  temp_forecast[4]       = {};
  int    wmo_forecast[4]        = {};
  String time_forecast[4];
  float  wind_speed_forecast[4] = {};
  int    wind_dir_forecast[4]   = {};
  bool   regen_warnung = false;
};

struct PollenDaten {
  // Open-Meteo Air Quality (Stundenwerte)
  float birke    = -1.0f;
  float graeser  = -1.0f;
  float erle     = -1.0f;
  float beifuss  = -1.0f;
  float ambrosia = -1.0f;
  // DWD Pollenflug – heute / morgen / übermorgen (Stufen 0-3 als Float)
  float dwd_birke    = -1.0f; float dwd_birke_tmr    = -1.0f; float dwd_birke_da    = -1.0f;
  float dwd_hasel    = -1.0f; float dwd_hasel_tmr    = -1.0f; float dwd_hasel_da    = -1.0f;
  float dwd_erle     = -1.0f; float dwd_erle_tmr     = -1.0f; float dwd_erle_da     = -1.0f;
  float dwd_esche    = -1.0f; float dwd_esche_tmr    = -1.0f; float dwd_esche_da    = -1.0f;
  float dwd_graeser  = -1.0f; float dwd_graeser_tmr  = -1.0f; float dwd_graeser_da  = -1.0f;
  float dwd_roggen   = -1.0f; float dwd_roggen_tmr   = -1.0f; float dwd_roggen_da   = -1.0f;
  float dwd_beifuss  = -1.0f; float dwd_beifuss_tmr  = -1.0f; float dwd_beifuss_da  = -1.0f;
  float dwd_ambrosia = -1.0f; float dwd_ambrosia_tmr = -1.0f; float dwd_ambrosia_da = -1.0f;
};

struct WarnDaten {
  bool   aktiv = false;
  String typ;           // z.B. "FROST", "REGEN", "WIND"
  int    level = 0;     // 1=Vorinformation 2=Warnung 3=Stark 4=Extrem
  String titel;
  String beschreibung;
  String region;
};

WetterDaten wetter;
PollenDaten pollen;
WarnDaten   warnungen[5];
int         anzahl_warnungen = 0;

// ── DWD Warnkarte ────────────────────────────────────────────
static uint8_t*      warnkarteBuf  = nullptr;
static size_t        warnkarteSize = 0;
static lv_img_dsc_t  warnkarteDsc  = {};
static lv_obj_t*     imgWarnkarte    = nullptr;
static lv_obj_t*     lblWarnkarteHint = nullptr;
unsigned long        letztesWarnkarteUpdate = 0;

// ============================================================
//  Zustands-Variablen
// ============================================================
bool   wifi_verbunden         = false;
bool   portal_modus           = false;
bool   pollenWarnAktiv        = false;
bool   pollenWarnBestaetigt   = false;
bool   pollenWarnGezeigt      = false;
bool   regenWarnAktiv         = false;
bool   regenWarnBestaetigt    = false;
bool   regenWarnGezeigt       = false;
bool   dwdWarnAktiv           = false;
bool   dwdWarnBestaetigt      = false;

unsigned long letztesDatenUpdate  = 0;
unsigned long letztesPollenUpdate = 0;
unsigned long letztesWarnUpdate   = 0;
unsigned long letztesTouchZeit    = 0;
unsigned long letzteUhrzeit       = 0;
bool          dimmed              = false;

// ============================================================
//  LVGL-Grundsetup
// ============================================================
static lv_disp_draw_buf_t draw_buf;
// Mit 8MB PSRAM: großen Buffer nutzen (verbessert Rendering-Speed)
static lv_color_t* lvgl_buf1 = nullptr;
static lv_color_t* lvgl_buf2 = nullptr;
#define LVGL_BUF_LINES 40

static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels((lgfx::swap565_t*)color_p, w * h);
  tft.endWrite();
  lv_disp_flush_ready(drv);
}

static void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  static uint16_t lx = 0, ly = 0;
  static bool wasPressed = false;
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    data->state   = LV_INDEV_STATE_PR;
    data->point.x = lx = x;
    data->point.y = ly = y;
    if (!wasPressed) {
      // Nur beim echten Press-Start updaten, nicht bei gehaltenem Finger oder Rauschen
      letztesTouchZeit = millis();
      if (dimmed) {
        setBrightness(cfg.brightness);
        dimmed = false;
      }
    }
    wasPressed = true;
  } else {
    data->state   = LV_INDEV_STATE_REL;
    data->point.x = lx;
    data->point.y = ly;
    wasPressed = false;
  }
}

// ============================================================
//  Helligkeit – ESP-IDF LEDC direkt (nicht Arduino-Wrapper)
// ============================================================
#include <driver/ledc.h>
#define BL_LEDC_TIMER   LEDC_TIMER_1
#define BL_LEDC_CHANNEL LEDC_CHANNEL_1
#define BL_LEDC_BITS    LEDC_TIMER_8_BIT

static bool bl_initialized = false;
void setBrightness(int prozent) {
  uint8_t val = (uint8_t)map(constrain(prozent, 0, 100), 0, 100, 0, 255);
  if (!bl_initialized) {
    ledc_timer_config_t tc = {};
    tc.speed_mode      = LEDC_LOW_SPEED_MODE;
    tc.duty_resolution = BL_LEDC_BITS;
    tc.timer_num       = BL_LEDC_TIMER;
    tc.freq_hz         = 5000;
    tc.clk_cfg         = LEDC_AUTO_CLK;
    esp_err_t et = ledc_timer_config(&tc);

    ledc_channel_config_t cc = {};
    cc.gpio_num   = TFT_BL;
    cc.speed_mode = LEDC_LOW_SPEED_MODE;
    cc.channel    = BL_LEDC_CHANNEL;
    cc.timer_sel  = BL_LEDC_TIMER;
    cc.intr_type  = LEDC_INTR_DISABLE;
    cc.duty       = 0;
    cc.hpoint     = 0;
    esp_err_t ec = ledc_channel_config(&cc);
    Serial.printf("[BL] LEDC init: timer=%d channel=%d gpio=%d\n", et, ec, TFT_BL);
    bl_initialized = true;
  }
  esp_err_t es = ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, val);
  esp_err_t eu = ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
  Serial.printf("[BL] setBrightness %d%% → val=%d set=%d upd=%d\n", prozent, val, es, eu);
}

// ============================================================
//  Touch-Kalibrierung (Dauerdruck beim Start = Neukalibrierung)
// ============================================================
void touchKalibriereJetzt(uint16_t* calData) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.println("Touch-Kalibrierung");
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 55);
  tft.println("Bitte nacheinander die");
  tft.setCursor(10, 70);
  tft.println("4 Kreuzmarkierungen antippen.");
  delay(1200);
  tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 15);
  prefs.putBytes("data", calData, 8 * sizeof(uint16_t));
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, TFT_HEIGHT / 2 - 10);
  tft.println("Kalibriert! OK");
  delay(800);
}

void touchKalibrierung() {
  prefs.begin("touch_cal", false);
  bool calSaved = (prefs.getBytesLength("data") == 8 * sizeof(uint16_t));
  uint16_t calData[8];

  if (calSaved) {
    prefs.getBytes("data", calData, sizeof(calData));
    tft.setTouchCalibrate(calData);
    Serial.println("[Touch] Kalibrierung geladen");

    // 2,5 s Fenster: Finger halten → Neukalibrierung
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, TFT_HEIGHT / 2 - 12);
    tft.println("Finger halten =");
    tft.setCursor(10, TFT_HEIGHT / 2 + 12);
    tft.println("Neukalibrierung");

    bool forceRecal = false;
    uint32_t t0 = millis();
    while (millis() - t0 < 2500) {
      uint16_t tx, ty;
      if (tft.getTouch(&tx, &ty)) { forceRecal = true; break; }
      delay(30);
    }
    if (forceRecal) {
      touchKalibriereJetzt(calData);
      Serial.println("[Touch] Neukalibrierung gespeichert");
    }
  } else {
    // Erststart: echte Kalibrierung durchführen
    Serial.println("[Touch] Erststart – Kalibrierung nötig");
    touchKalibriereJetzt(calData);
    Serial.println("[Touch] Erstkalibrierung gespeichert");
  }

  prefs.end();
}

// ============================================================
//  Konfiguration laden / speichern
// ============================================================
void ladeCfg() {
  prefs.begin("wcp", true);
  cfg.ssid            = prefs.getString("ssid",       "");
  cfg.pass            = prefs.getString("pass",       "");
  cfg.location        = prefs.getString("location",   "");
  cfg.lat             = prefs.getFloat ("lat",         0.0f);
  cfg.lon             = prefs.getFloat ("lon",         0.0f);
  cfg.dwd_region      = prefs.getInt   ("dwd_region",  31);
  cfg.regen_warn      = prefs.getBool  ("regen_warn",  true);
  cfg.pollen_warn     = prefs.getBool  ("pollen_warn", true);
  cfg.pollen_schwelle = prefs.getInt   ("pol_schw",    2);
  cfg.dim_timeout     = prefs.getInt   ("dim_time",    3);
  cfg.brightness      = prefs.getInt   ("bright",      80);
  cfg.warn_region     = prefs.getBool  ("warn_reg",    true);
  prefs.end();
}

void speichereCfg() {
  prefs.begin("wcp", false);
  prefs.putString("ssid",       cfg.ssid);
  prefs.putString("pass",       cfg.pass);
  prefs.putString("location",   cfg.location);
  prefs.putFloat ("lat",        cfg.lat);
  prefs.putFloat ("lon",        cfg.lon);
  prefs.putInt   ("dwd_region", cfg.dwd_region);
  prefs.putBool  ("regen_warn", cfg.regen_warn);
  prefs.putBool  ("pollen_warn",cfg.pollen_warn);
  prefs.putInt   ("pol_schw",   cfg.pollen_schwelle);
  prefs.putInt   ("dim_time",   cfg.dim_timeout);
  prefs.putInt   ("bright",     cfg.brightness);
  prefs.putBool  ("warn_reg",   cfg.warn_region);
  prefs.end();
}

// ============================================================
//  Geocoding
// ============================================================
bool geocodeLocation(const String& city) {
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=";
  url += city + "&count=1&language=de&format=json";
  http.begin(sc, url);
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      if (doc["results"].size() > 0) {
        cfg.lat = doc["results"][0]["latitude"].as<float>();
        cfg.lon = doc["results"][0]["longitude"].as<float>();
        http.end(); return true;
      }
    }
  }
  http.end();
  return false;
}

// ============================================================
//  Captive Portal – WLAN-Ersteinrichtung (deutsch)
// ============================================================
// HTML-Strings sind in webui_html.h definiert (Raw-String-Fix für Arduino-Präprozessor)

String baueDwdRegionenOptions(int ausgewaehlt) {
  String html = "";
  for (int i = 0; DWD_REGIONS[i].name != nullptr; i++) {
    html += "<option value=\"" + String(DWD_REGIONS[i].id) + "\"";
    if (DWD_REGIONS[i].id == ausgewaehlt) html += " selected";
    html += ">" + String(DWD_REGIONS[i].name) + "</option>\n";
  }
  return html;
}

void handlePortalRoot() {
  // WLAN-Scan
  int n = WiFi.scanNetworks();
  String nets = "";
  for (int i = 0; i < n; i++) {
    nets += "<option value=\"" + WiFi.SSID(i) + "\">" +
            WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)</option>\n";
  }

  String html = FPSTR(PORTAL_HTML);
  html.replace("%NETZWERKE%", nets);
  html.replace("%LOCATION%", cfg.location);
  html.replace("%DWD_REGIONEN%", baueDwdRegionenOptions(cfg.dwd_region));
  server.send(200, "text/html", html);
}

void handlePortalSave() {
  if (server.hasArg("ssid"))       cfg.ssid       = server.arg("ssid");
  if (server.hasArg("pass"))       cfg.pass       = server.arg("pass");
  if (server.hasArg("location"))   cfg.location   = server.arg("location");
  if (server.hasArg("dwd_region")) cfg.dwd_region = server.arg("dwd_region").toInt();
  cfg.lat = 0.0f; cfg.lon = 0.0f;  // erzwingt Geocoding beim nächsten Start
  speichereCfg();
  server.send(200, "text/html", FPSTR(PORTAL_OK_HTML));
  delay(2000);
  ESP.restart();
}

void startePortal() {
  portal_modus = true;
  WiFi.disconnect(true); delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("WetterCubePlus-Setup");
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/",          HTTP_GET,  handlePortalRoot);
  server.on("/speichern", HTTP_POST, handlePortalSave);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302); });
  server.begin();

  // Boot-Screen mit Portal-Hinweis über LVGL (kein raw TFT, damit Buttons aktiv bleiben)
  zeigeBootScreen("WLAN: WetterCubePlus-Setup");
  setzeBootFortschritt(0);
  Serial.println("[Portal] AP gestartet: WetterCubePlus-Setup");
}

// ============================================================
//  Web-Konfiguration (Normalbetrieb) – wettercubeplus.local
// ============================================================

// Vorwärts-Deklarationen
void handleWebRoot();
void handleWebSave();
void handleWebOtaCheck();
void handleWebOtaDoUpdate();
void handleWebWlanAendern();
void handleWebWlanSave();

void handleWebRoot() {
  String html = FPSTR(WEBUI_HTML);
  html.replace("%VERSION%",     FIRMWARE_VERSION);
  html.replace("%IP%",          WiFi.localIP().toString());
  html.replace("%LOCATION%",    cfg.location);
  html.replace("%DWD_REGIONEN%",baueDwdRegionenOptions(cfg.dwd_region));
  html.replace("%RW%",          cfg.regen_warn    ? "checked" : "");
  html.replace("%PW%",          cfg.pollen_warn   ? "checked" : "");
  html.replace("%WR%",          cfg.warn_region   ? "checked" : "");
  html.replace("%BRIGHT%",      String(cfg.brightness));
  html.replace("%DTT%", (cfg.dim_timeout == -1) ? "selected" : "");
  for (int i = 0; i <= 10; i++) {
    String key = "%DT" + String(i) + "%";
    html.replace(key, (cfg.dim_timeout == i) ? "selected" : "");
  }
  for (int i = 0; i <= 3; i++) {
    String key = "%PS" + String(i) + "%";
    html.replace(key, (cfg.pollen_schwelle == i) ? "selected" : "");
  }
  server.send(200, "text/html", html);
}

void handleWebSave() {
  String prevLocation = cfg.location;
  if (server.hasArg("location"))   cfg.location        = server.arg("location");
  if (server.hasArg("dwd_region")) cfg.dwd_region      = server.arg("dwd_region").toInt();
  cfg.regen_warn      = server.hasArg("regen_warn");
  cfg.pollen_warn     = server.hasArg("pollen_warn");
  cfg.warn_region     = server.hasArg("warn_region");
  if (server.hasArg("pol_schw"))   cfg.pollen_schwelle = server.arg("pol_schw").toInt();
  if (server.hasArg("brightness")) cfg.brightness      = server.arg("brightness").toInt();
  if (server.hasArg("dim_time"))   cfg.dim_timeout     = server.arg("dim_time").toInt();
  // Geocoding nur neu anstoßen wenn Location tatsächlich geändert
  if (cfg.location != prevLocation) { cfg.lat = 0.0f; cfg.lon = 0.0f; }
  speichereCfg();
  setBrightness(cfg.brightness);
  server.sendHeader("Location", "/"); server.send(302);
}

void handleWebOtaCheck() {
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  http.begin(sc, OTA_VERSION_URL);
  String latest = FIRMWARE_VERSION;
  bool update_available = false;
  if (http.GET() == 200) {
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      latest = doc["version"].as<String>();
      update_available = (latest != FIRMWARE_VERSION);
    }
  }
  http.end();
  String json = "{\"current\":\"" + String(FIRMWARE_VERSION) + "\",\"latest\":\"" +
                latest + "\",\"update_available\":" +
                (update_available ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleWebOtaDoUpdate() {
  Serial.printf("[OTA] Lade: %s\n", OTA_BIN_URL);

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.begin(client, OTA_BIN_URL);
  http.setTimeout(30000);
  int code = http.GET();
  Serial.printf("[OTA] HTTP rc=%d\n", code);
  if (code != 200) {
    http.end();
    Serial.printf("[OTA] Fehler: HTTP %d\n", code);
    server.send(502, "text/plain", "download error " + String(code));
    return;
  }

  int contentLen = http.getSize();
  WiFiClient* stream = http.getStreamPtr();
  Serial.printf("[OTA] %d Bytes\n", contentLen);

  if (!Update.begin(contentLen > 0 ? contentLen : UPDATE_SIZE_UNKNOWN)) {
    http.end();
    server.send(500, "text/plain", "begin failed");
    return;
  }
  size_t written = Update.writeStream(*stream);
  bool success = Update.end(true) && !Update.hasError();
  http.end();

  if (success) {
    Serial.printf("[OTA] %u Bytes – Neustart\n", written);
    server.send(200, "text/plain", "ok");
    delay(1500);
    ESP.restart();
  } else {
    Serial.printf("[OTA] Fehler: %s\n", Update.errorString());
    server.send(500, "text/plain", Update.errorString());
  }
}

void handleWebWlanAendern() {
  int n = WiFi.scanNetworks();
  String nets = "";
  for (int i = 0; i < n; i++) {
    nets += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</option>\n";
  }
  String html = FPSTR(WLAN_HTML);
  html.replace("%NETZWERKE%", nets);
  server.send(200, "text/html", html);
}

void handleWebWlanSave() {
  if (server.hasArg("ssid")) cfg.ssid = server.arg("ssid");
  if (server.hasArg("pass")) cfg.pass = server.arg("pass");
  cfg.lat = 0.0f; cfg.lon = 0.0f;
  speichereCfg();
  server.send(200, "text/html",
    "<!DOCTYPE html><html><body style='background:#0d1117;color:#e6edf3;font-family:Arial;padding:30px'>"
    "<h2 style='color:#2ea043'>WLAN gespeichert</h2><p>Neustart…</p></body></html>");
  delay(2000);
  ESP.restart();
}

void starteWebUI() {
  server.on("/",            HTTP_GET,  handleWebRoot);
  server.on("/speichern",   HTTP_POST, handleWebSave);
  server.on("/ota_check",   HTTP_GET,  handleWebOtaCheck);
  server.on("/ota_update",  HTTP_POST, handleWebOtaDoUpdate);
  server.on("/wlan",        HTTP_GET,  handleWebWlanAendern);
  server.on("/wlan_speichern", HTTP_POST, handleWebWlanSave);
  server.begin();
  Serial.println("[WebUI] Gestartet auf wettercubeplus.local");
}

// ============================================================
//  Daten abrufen
// ============================================================

// -- Hilfsfunktionen Pollen --
static float parseDwdWert(const char* s) {
  if (!s || strlen(s) == 0) return -1.0f;
  String str(s); str.trim();
  if (str == "-1") return -1.0f;
  int dash = str.indexOf('-');
  if (dash > 0) {
    float a = str.substring(0, dash).toFloat();
    float b = str.substring(dash + 1).toFloat();
    return (a + b) / 2.0f;
  }
  return str.toFloat();
}

const char* pollenText(float v) {
  if (v < 0)    return "--";
  if (v == 0)   return "keine";
  if (v <= 1.0) return "gering";
  if (v <= 2.0) return "mittel";
  if (v <= 2.5) return "hoch";
  return               "stark";
}

lv_color_t pollenColor(float v) {
  if (v <= 0)   return lv_color_hex(0x888888);
  if (v <= 1.0) return lv_color_hex(0x00CC44);
  if (v <= 2.0) return lv_color_hex(0xFFCC00);
  if (v <= 2.5) return lv_color_hex(0xFF8800);
  return               lv_color_hex(0xFF3300);
}

lv_color_t tempColor(float t) {
  if (t < 8)   return lv_color_hex(0x00AAFF);  // blau
  if (t < 15)  return lv_color_hex(0x00CC44);  // grün
  if (t < 24)  return lv_color_hex(0xFFCC00);  // gelb
  return              lv_color_hex(0xFF3300);   // rot
}

const lv_img_dsc_t* wmoZuImage(int wmo) {
  if (wmo <= 1)   return &day_clear;   // Klar
  if (wmo <= 3)   return &overcast;    // Teilweise / Stark bewölkt
  if (wmo <= 48)  return &overcast;    // Bewölkung, Nebel, gefrierender Nebel
  if (wmo <= 67)  return &rain;        // Nieselregen / Regen
  if (wmo <= 77)  return &snow;        // Schnee
  if (wmo <= 82)  return &rain;        // Regenschauer
  if (wmo <= 86)  return &snow;        // Schneeschauer
  return &thunder;                     // Gewitter
}

const char* windRichtung(int deg) {
  const char* dirs[] = {"N","NO","O","SO","S","SW","W","NW"};
  return dirs[((deg + 22) % 360) / 45];
}

// -- Wetterdaten (Open-Meteo) --
void fetchWetter() {
  if (WiFi.status() != WL_CONNECTED || cfg.lat == 0.0f) return;
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast";
  url += "?latitude="  + String(cfg.lat, 4);
  url += "&longitude=" + String(cfg.lon, 4);
  url += "&current=temperature_2m,relative_humidity_2m,apparent_temperature";
  url += ",wind_speed_10m,wind_direction_10m,surface_pressure,weather_code";
  url += "&daily=uv_index_max,sunrise,sunset";
  url += "&hourly=temperature_2m,weather_code,wind_speed_10m,wind_direction_10m";
  url += "&timezone=auto&forecast_days=2";
  http.begin(sc, url);
  if (http.GET() == 200) {
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      auto c = doc["current"];
      wetter.temp       = c["temperature_2m"].as<float>();
      wetter.feels_like = c["apparent_temperature"].as<float>();
      wetter.humidity   = c["relative_humidity_2m"].as<float>();
      wetter.wind_speed = c["wind_speed_10m"].as<float>();
      wetter.wind_dir   = c["wind_direction_10m"].as<int>();
      wetter.pressure   = c["surface_pressure"].as<float>();
      wetter.wmo_code   = c["weather_code"].as<int>();
      wetter.uv_index   = doc["daily"]["uv_index_max"][0].as<float>();
      // Sunrise/Sunset als String (ISO)
      String sr = doc["daily"]["sunrise"][0].as<String>();
      String ss = doc["daily"]["sunset"][0].as<String>();
      wetter.sunrise = sr.length() >= 16 ? sr.substring(11, 16) : sr;
      wetter.sunset  = ss.length() >= 16 ? ss.substring(11, 16) : ss;
      // 3h-Forecast
      struct tm ti; getLocalTime(&ti);
      int h = ti.tm_hour;
      for (int i = 0; i < 4; i++) {
        int idx = h + 1 + i;
        wetter.temp_forecast[i]       = doc["hourly"]["temperature_2m"][idx].as<float>();
        wetter.wmo_forecast[i]        = doc["hourly"]["weather_code"][idx].as<int>();
        wetter.wind_speed_forecast[i] = doc["hourly"]["wind_speed_10m"][idx].as<float>();
        wetter.wind_dir_forecast[i]   = doc["hourly"]["wind_direction_10m"][idx].as<int>();
        String t_str = doc["hourly"]["time"][idx].as<String>();
        wetter.time_forecast[i] = t_str.length() >= 16 ? t_str.substring(11, 16) : t_str;
      }
      // Regenwarnung: WMO >= 51 in aktueller oder nächster Stunde
      bool regen = false;
      for (int i = 0; i < 2; i++) {
        if (doc["hourly"]["weather_code"][h + i].as<int>() >= 51) regen = true;
      }
      bool vorher = wetter.regen_warnung;
      wetter.regen_warnung = regen;
      if (!vorher && regen) { regenWarnBestaetigt = false; regenWarnGezeigt = false; }
    }
  }
  http.end();
}

// -- DWD Pollen (Stufenwarnung) --
void fetchDwdPollen() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  http.begin(sc, "https://opendata.dwd.de/climate_environment/health/alerts/s31fg.json");
  if (http.GET() == 200) {
    StaticJsonDocument<128> filter;
    JsonArray fc = filter.createNestedArray("content");
    JsonObject fe = fc.createNestedObject();
    fe["region_id"] = true; fe["Pollen"] = true;
    DynamicJsonDocument doc(8192);
    String body = http.getString();
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter)) == DeserializationError::Ok) {
      for (JsonObject region : doc["content"].as<JsonArray>()) {
        if (region["region_id"].as<int>() != cfg.dwd_region) continue;
        JsonObject p = region["Pollen"];
        pollen.dwd_birke        = parseDwdWert(p["Birke"]["today"]         | "");
        pollen.dwd_birke_tmr    = parseDwdWert(p["Birke"]["tomorrow"]      | "");
        pollen.dwd_birke_da     = parseDwdWert(p["Birke"]["dayafter_to"]   | "");
        pollen.dwd_hasel        = parseDwdWert(p["Hasel"]["today"]         | "");
        pollen.dwd_hasel_tmr    = parseDwdWert(p["Hasel"]["tomorrow"]      | "");
        pollen.dwd_hasel_da     = parseDwdWert(p["Hasel"]["dayafter_to"]   | "");
        pollen.dwd_erle         = parseDwdWert(p["Erle"]["today"]          | "");
        pollen.dwd_erle_tmr     = parseDwdWert(p["Erle"]["tomorrow"]       | "");
        pollen.dwd_erle_da      = parseDwdWert(p["Erle"]["dayafter_to"]    | "");
        pollen.dwd_esche        = parseDwdWert(p["Esche"]["today"]         | "");
        pollen.dwd_esche_tmr    = parseDwdWert(p["Esche"]["tomorrow"]      | "");
        pollen.dwd_esche_da     = parseDwdWert(p["Esche"]["dayafter_to"]   | "");
        pollen.dwd_graeser      = parseDwdWert(p["Graeser"]["today"]       | "");
        pollen.dwd_graeser_tmr  = parseDwdWert(p["Graeser"]["tomorrow"]    | "");
        pollen.dwd_graeser_da   = parseDwdWert(p["Graeser"]["dayafter_to"] | "");
        pollen.dwd_roggen       = parseDwdWert(p["Roggen"]["today"]        | "");
        pollen.dwd_roggen_tmr   = parseDwdWert(p["Roggen"]["tomorrow"]     | "");
        pollen.dwd_roggen_da    = parseDwdWert(p["Roggen"]["dayafter_to"]  | "");
        pollen.dwd_beifuss      = parseDwdWert(p["Beifuss"]["today"]       | "");
        pollen.dwd_beifuss_tmr  = parseDwdWert(p["Beifuss"]["tomorrow"]    | "");
        pollen.dwd_beifuss_da   = parseDwdWert(p["Beifuss"]["dayafter_to"] | "");
        pollen.dwd_ambrosia     = parseDwdWert(p["Ambrosia"]["today"]      | "");
        pollen.dwd_ambrosia_tmr = parseDwdWert(p["Ambrosia"]["tomorrow"]   | "");
        pollen.dwd_ambrosia_da  = parseDwdWert(p["Ambrosia"]["dayafter_to"]| "");
        break;
      }
    }
  }
  http.end();
}

// -- DWD Wetterwarnungen --
// Endpoint: opendata.dwd.de (WARNUNG nach Bundesland/Region)
// Liefert aktive CAP-Warnungen als JSON; wir filtern nach Bundesland
void fetchDwdWarnungen() {
  if (WiFi.status() != WL_CONNECTED || !cfg.warn_region) return;
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  // Warnungen aggregiert nach Landkreis/Bundesland
  // URL liefert strukturiertes JSON mit Warnungen
  http.begin(sc, "https://www.dwd.de/DWD/warnungen/warnapp_gemeinden/json/warnings.json");
  http.addHeader("User-Agent", "WetterCubePlus/1.0");
  int code = http.GET();
  anzahl_warnungen = 0;

  if (code == 200) {
    // Warnungen-JSON ist groß (~100-500KB) – mit PSRAM als Stream parsen
    // Struktur: {"warnings":{"AGS_CODE":[{event,headline,description,severity,urgency}]}}
    // Da der volle Parse zu viel RAM braucht, filtern wir nach region_id-Prefix
    // Einfachere Alternative: Nur ersten 10 Warnungen laden
    DynamicJsonDocument doc(32768);  // 32KB mit PSRAM kein Problem
    DeserializationError err = deserializeJson(doc, http.getStream());
    if (err == DeserializationError::Ok) {
      int count = 0;
      JsonObject warnings = doc["warnings"].as<JsonObject>();
      for (JsonPair kv : warnings) {
        if (count >= 5) break;
        JsonArray arr = kv.value().as<JsonArray>();
        if (arr.size() == 0) continue;
        JsonObject w = arr[0].as<JsonObject>();
        int sev = w["severity"].as<int>();
        if (sev < 2) continue;  // nur Warnungen, keine Vorinformationen
        warnungen[count].aktiv       = true;
        warnungen[count].level       = sev;
        warnungen[count].typ         = w["event"].as<String>();
        warnungen[count].titel       = w["headline"].as<String>();
        warnungen[count].beschreibung= w["description"].as<String>();
        count++;
      }
      anzahl_warnungen = count;
    }
  }
  http.end();

  // Alternativ: Warnungen nach Bundesland via NINA-API des BBK (sehr gut strukturiert)
  // https://nina.api.proxy.bund.dev/api31/dwd/mapData.json
  // Liefert {id, payload{data{headline,msgType,severity,areas[]}}} – leichter zu parsen
  // TODO: NINA-API als zweite Option implementieren wenn DWD-JSON zu groß
}

// -- DWD Warnkarte (PNG → RGB565A8 im PSRAM) --
void fetchDwdWarnkarte() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  // DWD bietet nur eine Deutschland-Gesamtkarte als PNG an
  const char* url = "https://www.dwd.de/DWD/warnungen/warnapp_gemeinden/json/warnungen_gemeinde_map_de.png";
  Serial.printf("[Warnkarte] URL: %s\n", url);
  http.begin(sc, url);
  http.addHeader("User-Agent", "WetterCubePlus/1.0");
  http.setTimeout(20000);
  int httpCode = http.GET();
  Serial.printf("[Warnkarte] HTTP %d\n", httpCode);
  if (httpCode != 200) { http.end(); return; }

  const size_t MAXBUF = 400 * 1024;
  uint8_t* pngBuf = (uint8_t*)ps_malloc(MAXBUF);
  if (!pngBuf) { Serial.println("[Warnkarte] ps_malloc PNG fehlgeschlagen"); http.end(); return; }

  WiFiClient* stream = http.getStreamPtr();
  stream->setTimeout(15000);  // 15s Timeout pro Chunk (HTTPS/chunked: available() kann kurz 0 sein)
  size_t got = 0;
  unsigned long tStart = millis();
  while (got < MAXBUF && (millis() - tStart) < 20000) {
    size_t chunk = stream->readBytes(pngBuf + got, min((size_t)4096, MAXBUF - got));
    if (chunk > 0) {
      got += chunk;
    } else if (!stream->connected()) {
      break;  // Verbindung geschlossen = Download fertig
    } else {
      delay(5);
    }
  }
  http.end();
  Serial.printf("[Warnkarte] Download: %u Bytes in %lums\n", got, millis() - tStart);

  if (got < 512) { Serial.printf("[Warnkarte] Zu wenig Daten: %u\n", got); free(pngBuf); return; }
  Serial.printf("[Warnkarte] %u Bytes geladen, dekodiere PNG\n", got);

  // PNG → RGBA8888 (lodepng nutzt malloc; mit PSRAM als Heap-Erweiterung reicht der Speicher)
  unsigned char* rgba = nullptr;
  unsigned imgW = 0, imgH = 0;
  unsigned err = lodepng_decode32(&rgba, &imgW, &imgH, pngBuf, got);
  free(pngBuf);

  if (err || !rgba) {
    Serial.printf("[Warnkarte] lodepng Fehler %u\n", err);
    if (rgba) free(rgba);
    return;
  }
  Serial.printf("[Warnkarte] Dekodiert: %ux%u\n", imgW, imgH);

  // RGBA8888 → RGB565A8 im PSRAM (Format: [RGB565 block][Alpha block])
  // Mit LV_COLOR_16_SWAP=1: Bytes im RGB565-Word tauschen
  size_t rgb565Size  = (size_t)imgW * imgH * 2;
  size_t alphaSize   = (size_t)imgW * imgH;
  size_t totalSize   = rgb565Size + alphaSize;
  uint8_t* decoded = (uint8_t*)ps_malloc(totalSize);
  if (!decoded) {
    Serial.println("[Warnkarte] ps_malloc decoded fehlgeschlagen");
    free(rgba); return;
  }

  uint16_t* rgbPart   = (uint16_t*)decoded;
  uint8_t*  alphaPart = decoded + rgb565Size;
  for (size_t i = 0; i < (size_t)imgW * imgH; i++) {
    uint8_t r = rgba[i*4+0], g = rgba[i*4+1], b = rgba[i*4+2], a = rgba[i*4+3];
    uint16_t raw = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
    rgbPart[i]   = (raw >> 8) | (raw << 8);  // byte-swap für LV_COLOR_16_SWAP=1
    alphaPart[i] = a;
  }
  free(rgba);

  if (warnkarteBuf) free(warnkarteBuf);
  warnkarteBuf  = decoded;
  warnkarteSize = totalSize;

  memset(&warnkarteDsc, 0, sizeof(warnkarteDsc));
  warnkarteDsc.header.cf          = LV_IMG_CF_RGB565A8;
  warnkarteDsc.header.w           = imgW;
  warnkarteDsc.header.h           = imgH;
  warnkarteDsc.header.always_zero = 0;
  warnkarteDsc.data_size          = totalSize;
  warnkarteDsc.data               = warnkarteBuf;

  if (imgWarnkarte) {
    lv_img_set_src(imgWarnkarte, &warnkarteDsc);
    // Auf Display-Breite skalieren
    // Fit-to-screen: kleinster Zoom sodass Bild komplett ins Display passt
    // 40px Platz für Navigation-Buttons unten abziehen
    uint16_t zoomW = (uint16_t)((256.0f * TFT_WIDTH)        / imgW);
    uint16_t zoomH = (uint16_t)((256.0f * (TFT_HEIGHT - 40)) / imgH);
    uint16_t zoom  = min(zoomW, zoomH);
    lv_img_set_zoom(imgWarnkarte, zoom);
    lv_img_set_antialias(imgWarnkarte, false);
    lv_obj_align(imgWarnkarte, LV_ALIGN_CENTER, 0, 0);
    if (lblWarnkarteHint) lv_obj_add_flag(lblWarnkarteHint, LV_OBJ_FLAG_HIDDEN);
    Serial.printf("[Warnkarte] Angezeigt mit zoom=%u\n", zoom);
  }
}

void erstelleWarnkarteScreen() {
  if (!objects.screenwarnkarte1) return;
  // Hint zuerst, Bild danach → Bild liegt im Z-Order oben
  lblWarnkarteHint = lv_label_create(objects.screenwarnkarte1);
  if (lblWarnkarteHint) {
    lv_label_set_text(lblWarnkarteHint, "Lade Warnkarte ...");
    lv_obj_align(lblWarnkarteHint, LV_ALIGN_CENTER, 0, 0);
  }
  imgWarnkarte = lv_img_create(objects.screenwarnkarte1);
  if (imgWarnkarte) {
    lv_obj_align(imgWarnkarte, LV_ALIGN_CENTER, 0, 0);
  }
}

// -- Open-Meteo Pollen --
void fetchOpenMeteoPollen() {
  if (WiFi.status() != WL_CONNECTED || cfg.lat == 0.0f) return;
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  String url = "https://air-quality-api.open-meteo.com/v1/air-quality";
  url += "?latitude="  + String(cfg.lat, 4);
  url += "&longitude=" + String(cfg.lon, 4);
  url += "&hourly=birch_pollen,grass_pollen,alder_pollen,mugwort_pollen,ragweed_pollen";
  url += "&timezone=auto&forecast_days=1";
  http.begin(sc, url);
  if (http.GET() == 200) {
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      struct tm ti; getLocalTime(&ti);
      int h = ti.tm_hour;
      pollen.birke    = doc["hourly"]["birch_pollen"][h].as<float>();
      pollen.graeser  = doc["hourly"]["grass_pollen"][h].as<float>();
      pollen.erle     = doc["hourly"]["alder_pollen"][h].as<float>();
      pollen.beifuss  = doc["hourly"]["mugwort_pollen"][h].as<float>();
      pollen.ambrosia = doc["hourly"]["ragweed_pollen"][h].as<float>();
    }
  }
  http.end();
}

// ============================================================
//  UI-Update (PicoPixel-Widgets befüllen)
// ============================================================

// Setzt ein Label-Widget wenn es existiert
static void setLabel(lv_obj_t* obj, const char* text) {
  if (obj) lv_label_set_text(obj, text);
}
static void setLabelFmt(lv_obj_t* obj, lv_color_t col, const char* text) {
  if (!obj) return;
  lv_label_set_text(obj, text);
  lv_obj_set_style_text_color(obj, col, 0);
}

// Pollen-Label mit Farbe setzen
static void setPollenLabel(lv_obj_t* obj, float v) {
  if (!obj) return;
  lv_label_set_text(obj, pollenText(v));
  lv_obj_set_style_text_color(obj, pollenColor(v), 0);
}

// Schlimmsten Pollenverursacher (heute) als String
static String schleimsterPollen() {
  float max_v = -1.0f;
  const char* name = "Pollen";
  struct { float v; const char* n; } list[] = {
    {pollen.dwd_birke,   "Birke"},
    {pollen.dwd_hasel,   "Hasel"},
    {pollen.dwd_erle,    "Erle"},
    {pollen.dwd_esche,   "Esche"},
    {pollen.dwd_graeser, "Gräser"},
    {pollen.dwd_roggen,  "Roggen"},
    {pollen.dwd_beifuss, "Beifuß"},
    {pollen.dwd_ambrosia,"Ambrosia"},
  };
  for (auto& e : list) { if (e.v > max_v) { max_v = e.v; name = e.n; } }
  return String(name);
}

void aktualisiereUI() {
  struct tm ti;
  bool hasTime = getLocalTime(&ti);
  char buf[24];

  // ── Hauptscreen (screen_1) ──────────────────────────────────
  if (objects.labeltime && hasTime) {
    strftime(buf, sizeof(buf), "%H:%M", &ti);
    lv_label_set_text(objects.labeltime, buf);
  }
  if (objects.labeldatum && hasTime) {
    const char* wt[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
    snprintf(buf, sizeof(buf), "%s. %02d.%02d.%04d",
      wt[ti.tm_wday], ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
    lv_label_set_text(objects.labeldatum, buf);
  }
  if (objects.labeltemp) {
    snprintf(buf, sizeof(buf), "%.0f °C", wetter.temp);
    setLabelFmt(objects.labeltemp, tempColor(wetter.temp), buf);
  }
  if (objects.imagewetter) lv_img_set_src(objects.imagewetter, wmoZuImage(wetter.wmo_code));

  // screen_1: Wetter-Details (Feuchte, Wind, Druck)
  if (objects.labelhumidity) {
    snprintf(buf, sizeof(buf), "%.0f %%", wetter.humidity);
    lv_label_set_text(objects.labelhumidity, buf);
  }
  if (objects.labelwindspeed) {
    snprintf(buf, sizeof(buf), "%.0f km/h", wetter.wind_speed);
    lv_label_set_text(objects.labelwindspeed, buf);
  }
  if (objects.labelwinddirection) {
    lv_label_set_text(objects.labelwinddirection, windRichtung(wetter.wind_dir));
  }
  if (objects.labelairpressur) {
    snprintf(buf, sizeof(buf), "%.0f hPa", wetter.pressure);
    lv_label_set_text(objects.labelairpressur, buf);
  }

  // screen_1: Top-3 Pollen (DWD heute, absteigend sortiert)
  {
    struct PE { float v; const char* n; };
    PE pe[] = {
      {pollen.dwd_birke,    "Birke"},
      {pollen.dwd_hasel,    "Hasel"},
      {pollen.dwd_erle,     "Erle"},
      {pollen.dwd_esche,    "Esche"},
      {pollen.dwd_graeser,  "Gräser"},
      {pollen.dwd_roggen,   "Roggen"},
      {pollen.dwd_beifuss,  "Beifuß"},
      {pollen.dwd_ambrosia, "Ambrosia"},
    };
    for (int i = 0; i < 7; i++)
      for (int j = i+1; j < 8; j++)
        if (pe[j].v > pe[i].v) { PE tmp = pe[i]; pe[i] = pe[j]; pe[j] = tmp; }
    lv_obj_t* pn[] = {objects.labelpollenforecast1main,      objects.labelpollenforecast2main,      objects.labelpollenforecast3main};
    lv_obj_t* pv[] = {objects.labelpollenforecast1mainvalue, objects.labelpollenforecast2mainvalue, objects.labelpollenforecast3mainvalue};
    for (int i = 0; i < 3; i++) {
      setLabel(pn[i], pe[i].v >= 0 ? pe[i].n : "--");
      setPollenLabel(pv[i], pe[i].v);
    }
  }

  // ── Forecast Wetter (screenforecastwetter) ──────────────────
  lv_obj_t* hzeit[4]  = {objects.labelh1zeit,         objects.labelh2zeit,         objects.labelh3zeit,         objects.labelh4zeit};
  lv_obj_t* htemp[4]  = {objects.labelh1temp,         objects.labelh2temp,         objects.labelh3temp,         objects.labelh4temp};
  lv_obj_t* hws[4]    = {objects.labelh1windspeed,     objects.labelh2windspeed,    objects.labelh3windspeed,    objects.labelh4windspeed};
  lv_obj_t* hwd[4]    = {objects.labelh1winddirection, objects.labelh2winddirection,objects.labelh3winddirection,objects.labelh4winddirection};
  lv_obj_t* himg[4]   = {objects.imageh1,             objects.imageh2,             objects.imageh3,             objects.imageh4};

  for (int i = 0; i < 4; i++) {
    setLabel(hzeit[i], wetter.time_forecast[i].c_str());
    if (htemp[i]) {
      snprintf(buf, sizeof(buf), "%.0f °C", wetter.temp_forecast[i]);
      lv_label_set_text(htemp[i], buf);
    }
    if (hws[i]) {
      snprintf(buf, sizeof(buf), "%.0f km/h", wetter.wind_speed_forecast[i]);
      lv_label_set_text(hws[i], buf);
    }
    setLabel(hwd[i], windRichtung(wetter.wind_dir_forecast[i]));
    if (himg[i]) lv_img_set_src(himg[i], wmoZuImage(wetter.wmo_forecast[i]));
  }

  // ── Forecast Pollen (screenforecastpollen) ──────────────────
  // Datums-Spaltenköpfe
  if (hasTime) {
    time_t now_t = mktime(&ti);
    time_t t2_t = now_t + 86400;
    time_t t3_t = now_t + 172800;
    struct tm t1 = *localtime(&now_t);
    struct tm t2 = *localtime(&t2_t);
    struct tm t3 = *localtime(&t3_t);
    if (objects.labelday1) { strftime(buf, sizeof(buf), "%d.%m.", &t1); lv_label_set_text(objects.labelday1, buf); }
    if (objects.labelday2) { strftime(buf, sizeof(buf), "%d.%m.", &t2); lv_label_set_text(objects.labelday2, buf); }
    if (objects.labelday3) { strftime(buf, sizeof(buf), "%d.%m.", &t3); lv_label_set_text(objects.labelday3, buf); }
  }
  // Allergene
  setPollenLabel(objects.labelbirkeday1,    pollen.dwd_birke);
  setPollenLabel(objects.labelbirkeday2,    pollen.dwd_birke_tmr);
  setPollenLabel(objects.labelbirkeday3,    pollen.dwd_birke_da);
  setPollenLabel(objects.labelgraeserday1,  pollen.dwd_graeser);
  setPollenLabel(objects.labelgraeserday2,  pollen.dwd_graeser_tmr);
  setPollenLabel(objects.labelgraeserday3,  pollen.dwd_graeser_da);
  setPollenLabel(objects.labelerleday1,     pollen.dwd_erle);
  setPollenLabel(objects.labelerleday2,     pollen.dwd_erle_tmr);
  setPollenLabel(objects.labelerleday3,     pollen.dwd_erle_da);
  setPollenLabel(objects.labelhaselday1,    pollen.dwd_hasel);
  setPollenLabel(objects.labelhaselday2,    pollen.dwd_hasel_tmr);
  setPollenLabel(objects.labelhaselday3,    pollen.dwd_hasel_da);
  setPollenLabel(objects.labelescheday1,    pollen.dwd_esche);
  setPollenLabel(objects.labelescheday2,    pollen.dwd_esche_tmr);
  setPollenLabel(objects.labelescheday3,    pollen.dwd_esche_da);
  setPollenLabel(objects.labelroggenday1,   pollen.dwd_roggen);
  setPollenLabel(objects.labelroggenday2,   pollen.dwd_roggen_tmr);
  setPollenLabel(objects.labelroggenday3,   pollen.dwd_roggen_da);
  setPollenLabel(objects.labelbeifussday1,  pollen.dwd_beifuss);
  setPollenLabel(objects.labelbeifussday2,  pollen.dwd_beifuss_tmr);
  setPollenLabel(objects.labelbeifussday3,  pollen.dwd_beifuss_da);
  setPollenLabel(objects.labelambrosiaday1, pollen.dwd_ambrosia);
  setPollenLabel(objects.labelambrosiaday2, pollen.dwd_ambrosia_tmr);
  setPollenLabel(objects.labelambrosiaday3, pollen.dwd_ambrosia_da);

  // ── Regenwarnung (screenwarnung) ────────────────────────────
  regenWarnAktiv = cfg.regen_warn && wetter.regen_warnung;
  if (regenWarnAktiv) {
    setLabel(objects.labelwarntitel,  "Regenwarnung!");
    setLabel(objects.labelwarndetail, "Regen in den nächsten");
    setLabel(objects.labelwarndetail2,"60 Min.");
    setLabel(objects.labelwarnhint,   "Tippen zum Bestätigen");
  }

  // ── Pollenwarnung (screenwarnungpollen) ─────────────────────
  float pollenWerte[] = {pollen.dwd_birke, pollen.dwd_hasel, pollen.dwd_erle, pollen.dwd_esche,
                         pollen.dwd_graeser, pollen.dwd_roggen, pollen.dwd_beifuss, pollen.dwd_ambrosia};
  float maxPollen = -1.0f;
  for (int pi = 0; pi < 8; pi++) maxPollen = max(maxPollen, pollenWerte[pi]);
  pollenWarnAktiv = cfg.pollen_warn && (maxPollen > (float)cfg.pollen_schwelle);
  if (pollenWarnAktiv) {
    setLabel(objects.labelpollenwarntitel, "Pollenwarnung !");
    setLabel(objects.labelpollenwarnart,   schleimsterPollen().c_str());
    setLabel(objects.labelpollenwarnhint,  "Tippen zum Bestätigen");
  }
}

// ============================================================
//  Hinweis-Screen (programmatisch, für "noch nicht konfiguriert")
// ============================================================
static lv_obj_t* hinweisScreen = nullptr;

void zeigeHinweisScreen(const char* text) {
  if (!hinweisScreen) {
    hinweisScreen = lv_obj_create(nullptr);
    lv_obj_set_size(hinweisScreen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(hinweisScreen, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(hinweisScreen, LV_OPA_COVER, 0);

    lv_obj_t* titel = lv_label_create(hinweisScreen);
    // Kein Umlaut – LVGL Built-in-Fonts haben nur ASCII
    lv_label_set_text(titel, LV_SYMBOL_SETTINGS "  Einrichtung erforderlich");
    lv_obj_set_style_text_font(titel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(titel, lv_color_hex(0xf5a623), 0);
    lv_obj_align(titel, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t* lbl = lv_label_create(hinweisScreen);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, LV_HOR_RES - 40);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 72);
  }
  lv_scr_load(hinweisScreen);
  lv_timer_handler();
}

// ============================================================
//  Boot-Screen (PicoPixel screenboot)
// ============================================================
void lvgl_flush(uint32_t ms = 80) {
  uint32_t t = millis();
  while (millis() - t < ms) { lv_timer_handler(); delay(5); }
}

void zeigeBootScreen(const String& msg) {
  if (objects.labelstatus != nullptr)
    lv_label_set_text(objects.labelstatus, msg.c_str());
  lvgl_flush();
}

void setzeBootFortschritt(int prozent) {
  if (objects.barwifi != nullptr)
    lv_bar_set_value(objects.barwifi, prozent, LV_ANIM_ON);
  lvgl_flush();
}

// ============================================================
//  LVGL Event-Callbacks
// ============================================================

// screen_1 → forecastwetter → forecastpollen → screenwarnkarte1 → screen_1
static void cbHome(lv_event_t*)   { loadScreen(SCREEN_ID_SCREEN_1); }
static void cbFwd1(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENFORECASTWETTER); }   // screen_1 >
static void cbBack1(lv_event_t*) { loadScreen(SCREEN_ID_SCREEN_1); }               // forecastwetter <
static void cbFwd2(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENFORECASTPOLLEN); }   // forecastwetter >
static void cbBack2(lv_event_t*) { loadScreen(SCREEN_ID_SCREENFORECASTWETTER); }   // forecastpollen <
static void cbFwd3(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENWARNKARTE1); }       // forecastpollen >
static void cbBack3(lv_event_t*) { loadScreen(SCREEN_ID_SCREENFORECASTPOLLEN); }   // screenwarnkarte1 <
static void cbFwd4(lv_event_t*)  { loadScreen(SCREEN_ID_SCREEN_1); }               // screenwarnkarte1 >

// ── Blinken auf Warnscreens (500 ms, Icon + Titel) ──────────
static bool warnBlinkState = false;
static void cbWarnBlink(lv_timer_t*) {
  warnBlinkState = !warnBlinkState;
  lv_obj_t* aktiv = lv_scr_act();
  if (aktiv == objects.screenwarnung) {
    // Hintergrund blinkt zwischen Rot und Dunkelrot
    lv_color_t bg = warnBlinkState ? lv_color_hex(0xcc0000) : lv_color_hex(0x550000);
    lv_obj_set_style_bg_color(aktiv, bg, LV_PART_MAIN);
  } else if (aktiv == objects.screenwarnungpollen) {
    // Hintergrund blinkt zwischen Orange und Dunkelorange
    lv_color_t bg = warnBlinkState ? lv_color_hex(0xd67a28) : lv_color_hex(0x5a2f0a);
    lv_obj_set_style_bg_color(aktiv, bg, LV_PART_MAIN);
  }
}

static void cbWarnTap(lv_event_t* e) {
  lv_obj_t* src = lv_event_get_current_target(e);
  if (src == objects.screenwarnung) {
    regenWarnBestaetigt = true;
    loadScreen(SCREEN_ID_SCREEN_1);
  } else if (src == objects.screenwarnungpollen) {
    pollenWarnBestaetigt = true;
    loadScreen(SCREEN_ID_SCREEN_1);
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[WetterCubePlus] Start v" FIRMWARE_VERSION);

  // Konfiguration laden
  ladeCfg();

  // Display initialisieren
  tft.begin();
  tft.setRotation(1);  // Landscape (480x320)
  // BL-Test: 300ms dunkel → bestätigt dass LEDC funktioniert
  setBrightness(0);
  delay(300);
  setBrightness(cfg.brightness);
  tft.fillScreen(TFT_BLACK);

  // Touch-Kalibrierung (Finger halten beim Start = Neukalibrierung)
  touchKalibrierung();

  // LVGL initialisieren
  lv_init();
  // PSRAM für LVGL-Buffer nutzen
  lvgl_buf1 = (lv_color_t*)ps_malloc(TFT_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t));
  lvgl_buf2 = (lv_color_t*)ps_malloc(TFT_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t));
  if (!lvgl_buf1 || !lvgl_buf2) {
    // Fallback auf normalen RAM wenn PSRAM nicht verfügbar
    static lv_color_t fallback1[TFT_WIDTH * 10];
    static lv_color_t fallback2[TFT_WIDTH * 10];
    lv_disp_draw_buf_init(&draw_buf, fallback1, fallback2, TFT_WIDTH * 10);
  } else {
    lv_disp_draw_buf_init(&draw_buf, lvgl_buf1, lvgl_buf2, TFT_WIDTH * LVGL_BUF_LINES);
  }

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = TFT_WIDTH;
  disp_drv.ver_res  = TFT_HEIGHT;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.flush_cb = disp_flush;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_read;
  lv_indev_drv_register(&indev_drv);

  // PNG-Decoder registrieren (muss vor ui_init sein, sonst lv_img_set_src macht nichts)
  lv_png_init();

  // PicoPixel UI initialisieren, dann explizit screenboot laden
  ui_init();
  lv_scr_load(objects.screenboot);
  lvgl_flush();

  // Helles Theme: weißer Hintergrund + dunkle Schrift auf allen normalen Screens
  // (Warn-Screens behalten ihre Farben; screenboot bleibt schwarz)
  {
    lv_obj_t* normalScreens[] = {
      objects.screen_1, objects.screenforecastwetter,
      objects.screenforecastpollen, objects.screenwarnkarte1
    };
    for (int i = 0; i < 4; i++) {
      if (!normalScreens[i]) continue;
      lv_obj_set_style_bg_color(normalScreens[i],  lv_color_white(),        LV_PART_MAIN);
      lv_obj_set_style_text_color(normalScreens[i], lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    }
  }

  // Warn-Blink-Timer (läuft immer, wirkt nur wenn Warnscreen aktiv)
  lv_timer_create(cbWarnBlink, 500, nullptr);

  // Warnkarte-Screen programmatisch anlegen
  erstelleWarnkarteScreen();

  // Navigations-Buttons sofort verdrahten (vor WiFi-Check, gilt auch im Portal-Modus)
  // Navigation: screen_1 → forecastwetter → forecastpollen → screenwarnkarte1 → screen_1
  #define REG_CB(obj, cb, evt) do { if (obj) lv_obj_add_event_cb(obj, cb, evt, nullptr); } while(0)
  REG_CB(objects.labelbuttonforward,    cbFwd1,    LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonbackward,   cbBack1,   LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonforward_1,  cbFwd2,    LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonbackward_2, cbBack2,   LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonforward_2,  cbFwd3,    LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonbackward_1, cbBack3,   LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonforward_3,  cbFwd4,    LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonhome,   cbHome, LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonhome_1, cbHome, LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonhome_2, cbHome, LV_EVENT_CLICKED);
  REG_CB(objects.screenwarnung,         cbWarnTap, LV_EVENT_CLICKED);
  REG_CB(objects.screenwarnungpollen,   cbWarnTap, LV_EVENT_CLICKED);
  #undef REG_CB

  // WiFi verbinden oder Portal starten
  if (cfg.ssid.isEmpty()) {
    zeigeHinweisScreen("Bitte den WetterCube konfigurieren.\n\nVerbinde dein Geraet mit:\nWLAN: WetterCubePlus-Setup\n\nEs oeffnet sich ein Konfigurationsportal.\nFalls nicht: im Browser\nhttp://192.168.4.1 aufrufen.");
    startePortal();
    return;
  }

  zeigeBootScreen("Verbinde mit " + cfg.ssid + "...");
  setzeBootFortschritt(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    lvgl_flush(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    zeigeHinweisScreen("WLAN-Verbindung fehlgeschlagen.\n\nVerbinde dich mit:\nWLAN: WetterCubePlus-Setup\n\nEs oeffnet sich ein Konfigurationsportal.\nFalls nicht: im Browser\nhttp://192.168.4.1 aufrufen.\n\nSSID und Passwort pruefen.");
    delay(500);
    startePortal();
    return;
  }

  wifi_verbunden = true;
  setzeBootFortschritt(30);
  zeigeBootScreen("Verbunden: " + WiFi.localIP().toString());
  Serial.println("[WiFi] Verbunden: " + WiFi.localIP().toString());

  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] wettercubeplus.local aktiv");
  }

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "europe.pool.ntp.org");
  zeigeBootScreen("Synchronisiere Zeit...");
  setzeBootFortschritt(45);
  { struct tm ti; int r = 0; while (!getLocalTime(&ti) && r++ < 20) { delay(500); lv_timer_handler(); } }

  starteWebUI();
  setzeBootFortschritt(55);

  if (cfg.lat == 0.0f && !cfg.location.isEmpty()) {
    zeigeBootScreen("Koordinaten für " + cfg.location + "...");
    if (geocodeLocation(cfg.location)) speichereCfg();
    setzeBootFortschritt(65);
  }

  zeigeBootScreen("Lade Wetterdaten...");
  setzeBootFortschritt(70);
  fetchWetter();
  setzeBootFortschritt(80);
  zeigeBootScreen("Lade Pollenflug...");
  fetchDwdPollen();
  fetchOpenMeteoPollen();
  setzeBootFortschritt(90);
  zeigeBootScreen("Lade Warnungen...");
  fetchDwdWarnungen();
  setzeBootFortschritt(95);
  zeigeBootScreen("Lade Warnkarte...");
  fetchDwdWarnkarte();
  letztesWarnkarteUpdate = millis();
  setzeBootFortschritt(100);

  // UI vollständig befüllen bevor der Hauptscreen erscheint
  aktualisiereUI();
  // Kurz warten damit 100% sichtbar ist, dann Hauptscreen laden
  delay(400);
  // LVGL-Animation-Timing: 600ms lv_timer_handler() vor weiteren Calls (Lessons Learned!)
  { unsigned long e = millis() + 600; while (millis() < e) { lv_timer_handler(); delay(5); } }
  loadScreen(SCREEN_ID_SCREEN_1);


  letztesTouchZeit    = millis();
  letztesDatenUpdate  = millis();
  letztesPollenUpdate = millis();
  letztesWarnUpdate   = millis();
  letzteUhrzeit       = millis();

  Serial.println("[Setup] Bereit.");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  lv_timer_handler();
  ui_tick();

  // Portal-Modus: DNS + WebServer bedienen
  if (portal_modus) {
    dnsServer.processNextRequest();
    server.handleClient();
    return;
  }

  // WebServer
  server.handleClient();

  // Uhrzeit und Datum jede Minute
  if (millis() - letzteUhrzeit >= 60000) {
    letzteUhrzeit = millis();
    struct tm ti;
    if (getLocalTime(&ti)) {
      char buf[6];
      strftime(buf, sizeof(buf), "%H:%M", &ti);
      if (objects.labeltime) lv_label_set_text(objects.labeltime, buf);
      // Datum mitführen
      const char* wt[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
      char dbuf[24];
      snprintf(dbuf, sizeof(dbuf), "%s. %02d.%02d.%04d",
        wt[ti.tm_wday], ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
      if (objects.labeldatum) lv_label_set_text(objects.labeldatum, dbuf);
    }
  }

  // Wetterdaten alle 10 Minuten
  if (millis() - letztesDatenUpdate >= 600000UL) {
    letztesDatenUpdate = millis();
    fetchWetter();
    aktualisiereUI();
  }

  // Pollendaten alle 60 Minuten
  if (millis() - letztesPollenUpdate >= 3600000UL) {
    letztesPollenUpdate = millis();
    fetchDwdPollen();
    fetchOpenMeteoPollen();
    aktualisiereUI();
  }

  // Warnbildschirme einmalig anzeigen wenn neue Warnung
  if (regenWarnAktiv && !regenWarnBestaetigt && !regenWarnGezeigt) {
    regenWarnGezeigt = true;
    loadScreen(SCREEN_ID_SCREENWARNUNG);
  } else if (pollenWarnAktiv && !pollenWarnBestaetigt && !pollenWarnGezeigt) {
    pollenWarnGezeigt = true;
    loadScreen(SCREEN_ID_SCREENWARNUNGPOLLEN);
  }

  // DWD-Warnungen alle 15 Minuten
  if (millis() - letztesWarnUpdate >= 900000UL) {
    letztesWarnUpdate = millis();
    fetchDwdWarnungen();
  }

  // DWD-Warnkarte alle 30 Minuten
  if (millis() - letztesWarnkarteUpdate >= 1800000UL) {
    letztesWarnkarteUpdate = millis();
    fetchDwdWarnkarte();
  }

  // Display-Dimmen
  if (cfg.dim_timeout > 0 && !dimmed) {
    // dim_timeout -1 = 30 Sekunden (Testmodus), sonst Minuten
    unsigned long ms = (cfg.dim_timeout == -1) ? 30000UL : (unsigned long)cfg.dim_timeout * 60000UL;
    if (millis() - letztesTouchZeit >= ms) {
      Serial.printf("[Dim] Dimme: elapsed=%lus, timeout=%d\n",
                    (millis() - letztesTouchZeit) / 1000, cfg.dim_timeout);
      setBrightness(10);
      dimmed = true;
    }
  }

  delay(5);
}

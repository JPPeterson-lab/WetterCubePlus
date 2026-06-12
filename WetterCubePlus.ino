// ============================================================
//  WetterCubePlus.ino
//  ESP32-S3 N16R8 | ILI9488 3.5" 480x320 | XPT2046 Touch
//  LVGL 8.x | LovyanGFX | HTTP-OTA | WebUI | DWD-Warnungen
//  Version: 0.1.1-beta
// ============================================================

#include "webui_html.h"

// ---- Versions-Define (muss mit docs/version.json übereinstimmen!) ----
#define FIRMWARE_VERSION "0.1.1-beta"
#define OTA_VERSION_URL  "https://raw.githubusercontent.com/JPPeterson-lab/WetterCubePlus/main/docs/version.json"
#define OTA_BIN_BASE_URL "https://github.com/JPPeterson-lab/WetterCubePlus/releases/download/"
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
#include "ui/ui.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>

// ============================================================
//  LovyanGFX – ILI9488 + XPT2046
// ============================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488  _panel;
  lgfx::Bus_SPI        _bus;
  lgfx::Touch_XPT2046  _touch;
  lgfx::Light_PWM      _light;
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

    { auto cfg = _light.config();
      cfg.pin_bl  = TFT_BL;
      cfg.invert  = false;
      cfg.freq    = 5000;
      cfg.pwm_channel = 0;
      _light.config(cfg);
      _panel.setLight(&_light); }

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
  float  temp_forecast[4]  = {};
  int    wmo_forecast[4]   = {};
  String time_forecast[4];
  bool   regen_warnung = false;
};

struct PollenDaten {
  // Open-Meteo Air Quality (Stundenwerte)
  float birke    = -1.0f;
  float graeser  = -1.0f;
  float erle     = -1.0f;
  float beifuss  = -1.0f;
  float ambrosia = -1.0f;
  // DWD Pollenflug (Stufen 0-3 als Float nach parseDwdWert)
  float dwd_birke    = -1.0f;
  float dwd_hasel    = -1.0f;
  float dwd_erle     = -1.0f;
  float dwd_esche    = -1.0f;
  float dwd_graeser  = -1.0f;
  float dwd_roggen   = -1.0f;
  float dwd_beifuss  = -1.0f;
  float dwd_ambrosia = -1.0f;
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

// ============================================================
//  Zustands-Variablen
// ============================================================
bool   wifi_verbunden         = false;
bool   portal_modus           = false;
bool   pollenWarnAktiv        = false;
bool   pollenWarnBestaetigt   = false;
bool   regenWarnAktiv         = false;
bool   regenWarnBestaetigt    = false;
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
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    data->state   = LV_INDEV_STATE_PR;
    data->point.x = lx = x;
    data->point.y = ly = y;
    letztesTouchZeit = millis();
    if (dimmed) {
      setBrightness(cfg.brightness);
      dimmed = false;
    }
  } else {
    data->state   = LV_INDEV_STATE_REL;
    data->point.x = lx;
    data->point.y = ly;
  }
}

// ============================================================
//  Helligkeit
// ============================================================
void setBrightness(int prozent) {
  int val = map(constrain(prozent, 0, 100), 0, 100, 0, 255);
  tft.setBrightness(val);
}

// ============================================================
//  Touch-Kalibrierung (Dauerdruck beim Start = Neukalibrierung)
// ============================================================
// Prüft ob Touch überhaupt antwortet (10 Versuche)
bool touchErreichbar() {
  for (int i = 0; i < 10; i++) {
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty)) {
      Serial.printf("[Touch] Erreichbar: x=%d y=%d\n", tx, ty);
      return true;
    }
    delay(20);
  }
  return false;
}

void touchKalibrierung() {
  // Hint anzeigen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, TFT_HEIGHT / 2 - 30);
  tft.println("Touch-Test...");

  // Erst prüfen ob Touch-Chip überhaupt reagiert
  // XPT2046 Raw-SPI-Test: Finger auflegen, dann messen
  tft.setTextSize(1);
  tft.setCursor(20, TFT_HEIGHT / 2 + 10);
  tft.println("Finger auflegen zum Testen (3 Sek.)");

  bool touchOk = false;
  uint32_t t0 = millis();
  while (millis() - t0 < 3000) {
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty)) {
      Serial.printf("[Touch] OK: x=%d y=%d\n", tx, ty);
      touchOk = true;
      break;
    }
    delay(30);
  }

  if (!touchOk) {
    Serial.println("[Touch] KEIN Signal – Touch-SPI pruefen!");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 20);  tft.println("Touch: kein Signal");
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(10, 60);  tft.println("Pins pruefen:");
    tft.setCursor(10, 80);  tft.printf("T_CLK=%d T_DIN=%d T_DO=%d", TFT_SCLK, TFT_MOSI, TFT_MISO);
    tft.setCursor(10, 100); tft.printf("T_CS=%d  T_IRQ=%d", TOUCH_CS, TOUCH_IRQ);
    tft.setCursor(10, 130); tft.println("Weiter ohne Touch-Kalibrierung...");
    delay(3000);
    // Ohne Kalibrierung weitermachen – Touch wird im Betrieb nicht funktionieren
    // aber zumindest startet das Gerät
    return;
  }

  // Touch antwortet – jetzt auf Dauerdruck für Neukalibrierung prüfen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, TFT_HEIGHT / 2 - 20);
  tft.println("Finger halten =");
  tft.setCursor(20, TFT_HEIGHT / 2 + 5);
  tft.println("Neukalibrierung");

  bool forceRecal = false;
  t0 = millis();
  while (millis() - t0 < 2500) {
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty)) {
      forceRecal = true;
      break;
    }
    delay(30);
  }
  Serial.printf("[Touch] forceRecal=%d\n", forceRecal);

  prefs.begin("touch_cal", false);
  bool calSaved = (prefs.getBytesLength("data") == 8 * sizeof(uint16_t));

  if (!calSaved || forceRecal) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(20, TFT_HEIGHT / 2 - 10);
    tft.println("Ecken antippen...");
    delay(500);
    uint16_t calData[8];
    tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 15);
    prefs.putBytes("data", calData, sizeof(calData));
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(20, TFT_HEIGHT / 2 - 10);
    tft.println("Gespeichert!");
    delay(1000);
  } else {
    uint16_t calData[8];
    prefs.getBytes("data", calData, sizeof(calData));
    tft.setTouchCalibrate(calData);
    Serial.println("[Touch] Kalibrierung aus Preferences geladen");
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

  // Display-Hinweis
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW); tft.setTextSize(2);
  tft.setCursor(10, 20);  tft.println("Einrichtungsmodus");
  tft.setTextColor(TFT_WHITE); tft.setTextSize(1);
  tft.setCursor(10, 55);  tft.println("WLAN: WetterCubePlus-Setup");
  tft.setCursor(10, 70);  tft.println("Browser: 192.168.4.1");
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
  if (server.hasArg("location"))   cfg.location        = server.arg("location");
  if (server.hasArg("dwd_region")) cfg.dwd_region      = server.arg("dwd_region").toInt();
  cfg.regen_warn      = server.hasArg("regen_warn");
  cfg.pollen_warn     = server.hasArg("pollen_warn");
  cfg.warn_region     = server.hasArg("warn_region");
  if (server.hasArg("pol_schw"))   cfg.pollen_schwelle = server.arg("pol_schw").toInt();
  if (server.hasArg("brightness")) cfg.brightness      = server.arg("brightness").toInt();
  if (server.hasArg("dim_time"))   cfg.dim_timeout     = server.arg("dim_time").toInt();
  speichereCfg();
  setBrightness(cfg.brightness);
  // Geocoding neu anstoßen wenn Location geändert
  cfg.lat = 0.0f; cfg.lon = 0.0f;
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
  server.send(200, "text/plain", "Update gestartet, Gerät startet neu…");
  delay(500);

  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  http.begin(sc, OTA_VERSION_URL);
  String binUrl = "";
  if (http.GET() == 200) {
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      String ver = doc["version"].as<String>();
      binUrl = String(OTA_BIN_BASE_URL) + "v" + ver + "/WetterCubePlus-" + ver + ".bin";
    }
  }
  http.end();

  if (binUrl.isEmpty()) { Serial.println("[OTA] Keine URL"); return; }

  WiFiClientSecure sc2; sc2.setInsecure();
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  t_httpUpdate_return ret = httpUpdate.update(sc2, binUrl);
  if (ret != HTTP_UPDATE_OK) {
    Serial.printf("[OTA] Fehler: %s\n", httpUpdate.getLastErrorString().c_str());
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
  if (t < 0)   return lv_color_hex(0x00AAFF);
  if (t < 10)  return lv_color_hex(0x00CC44);
  if (t < 25)  return lv_color_hex(0xFFCC00);
  return              lv_color_hex(0xFF3300);
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
  url += "&hourly=temperature_2m,weather_code";
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
        wetter.temp_forecast[i] = doc["hourly"]["temperature_2m"][idx].as<float>();
        wetter.wmo_forecast[i]  = doc["hourly"]["weather_code"][idx].as<int>();
        String t_str = doc["hourly"]["time"][idx].as<String>();
        wetter.time_forecast[i] = t_str.length() >= 16 ? t_str.substring(11, 16) : t_str;
      }
      // Regenwarnung: WMO >= 51 in nächsten 2 Stunden
      bool regen = false;
      for (int i = 0; i < 2; i++) {
        if (doc["hourly"]["weather_code"][h + 1 + i].as<int>() >= 51) regen = true;
      }
      wetter.regen_warnung = regen;
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
        pollen.dwd_birke    = parseDwdWert(p["Birke"]["today"]    | "");
        pollen.dwd_hasel    = parseDwdWert(p["Hasel"]["today"]    | "");
        pollen.dwd_erle     = parseDwdWert(p["Erle"]["today"]     | "");
        pollen.dwd_esche    = parseDwdWert(p["Esche"]["today"]    | "");
        pollen.dwd_graeser  = parseDwdWert(p["Graeser"]["today"]  | "");
        pollen.dwd_roggen   = parseDwdWert(p["Roggen"]["today"]   | "");
        pollen.dwd_beifuss  = parseDwdWert(p["Beifuss"]["today"]  | "");
        pollen.dwd_ambrosia = parseDwdWert(p["Ambrosia"]["today"] | "");
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
void aktualisiereUI() {
  // Hauptscreen – Uhrzeit
  if (objects.labeltime != nullptr) {
    struct tm ti;
    if (getLocalTime(&ti)) {
      char buf[6];
      strftime(buf, sizeof(buf), "%H:%M", &ti);
      lv_label_set_text(objects.labeltime, buf);
    }
  }
  // TODO: weitere Widgets sobald in PicoPixel hinzugefügt
  // (Temperatur, Pollen, Warnungen – Widget-Namen aus screens.h)
}

// ============================================================
//  Boot-Screen (PicoPixel screenboot)
// ============================================================
void zeigeBootScreen(const String& msg) {
  if (objects.labelstatus != nullptr)
    lv_label_set_text(objects.labelstatus, msg.c_str());
  lv_timer_handler(); delay(10);
}

void setzeBootFortschritt(int prozent) {
  if (objects.barwifi != nullptr)
    lv_bar_set_value(objects.barwifi, prozent, LV_ANIM_ON);
  lv_timer_handler();
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

  // PicoPixel UI initialisieren – startet auf screenboot
  ui_init();
  lv_timer_handler();

  // WiFi verbinden oder Portal starten
  if (cfg.ssid.isEmpty()) {
    zeigeBootScreen("Kein WLAN – Portal startet…");
    startePortal();
    return;
  }

  zeigeBootScreen("Verbinde mit " + cfg.ssid + "…");
  setzeBootFortschritt(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(300);
    lv_timer_handler();
  }

  if (WiFi.status() != WL_CONNECTED) {
    zeigeBootScreen("WLAN fehlgeschlagen – Portal…");
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
  zeigeBootScreen("Synchronisiere Zeit…");
  setzeBootFortschritt(45);
  { struct tm ti; int r = 0; while (!getLocalTime(&ti) && r++ < 20) { delay(500); lv_timer_handler(); } }

  starteWebUI();
  setzeBootFortschritt(55);

  if (cfg.lat == 0.0f && !cfg.location.isEmpty()) {
    zeigeBootScreen("Koordinaten für " + cfg.location + "…");
    if (geocodeLocation(cfg.location)) speichereCfg();
    setzeBootFortschritt(65);
  }

  zeigeBootScreen("Lade Wetterdaten…");
  setzeBootFortschritt(70);
  fetchWetter();
  setzeBootFortschritt(80);
  zeigeBootScreen("Lade Pollenflug…");
  fetchDwdPollen();
  fetchOpenMeteoPollen();
  setzeBootFortschritt(90);
  zeigeBootScreen("Lade Warnungen…");
  fetchDwdWarnungen();
  setzeBootFortschritt(100);

  // Kurz warten damit 100% sichtbar ist, dann Hauptscreen laden
  delay(400);
  // LVGL-Animation-Timing: 600ms lv_timer_handler() vor weiteren Calls (Lessons Learned!)
  { unsigned long e = millis() + 600; while (millis() < e) { lv_timer_handler(); delay(5); } }
  loadScreen(SCREEN_ID_SCREEN_1);
  aktualisiereUI();

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

  // Uhrzeit alle 30 Sekunden
  if (millis() - letzteUhrzeit >= 30000) {
    letzteUhrzeit = millis();
    // TODO: lv_label_set_text(ui_uhrzeit, ...) nach UI-Integration
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
  }

  // DWD-Warnungen alle 15 Minuten
  if (millis() - letztesWarnUpdate >= 900000UL) {
    letztesWarnUpdate = millis();
    fetchDwdWarnungen();
  }

  // Display-Dimmen
  if (cfg.dim_timeout > 0 && !dimmed) {
    if (millis() - letztesTouchZeit >= (unsigned long)cfg.dim_timeout * 60000UL) {
      setBrightness(20);
      dimmed = true;
    }
  }

  delay(5);
}

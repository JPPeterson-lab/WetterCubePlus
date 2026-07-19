// ============================================================
//  WetterCubePlus.ino
//  ESP32-S3 N16R8 | ILI9488 3.5" 480x320 | XPT2046 Touch
//  LVGL 8.x | LovyanGFX | HTTP-OTA | WebUI | DWD-Warnungen
//  Version: 0.5.4-beta
// ============================================================

#include "webui_html.h"

// ---- Versions-Define (muss mit docs/version.json übereinstimmen!) ----
#define FIRMWARE_VERSION "0.8.0-beta"
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
  bool   night_mode = false;    // Nachtmodus ein/aus
  int    night_from = 22 * 60;  // Startzeit in Minuten seit Mitternacht (22:00)
  int    night_to   =  6 * 60;  // Endzeit   in Minuten seit Mitternacht (06:00)
  int    night_brightness = 10; // Helligkeit im Nachtmodus (%)
  // Ampel-Schwellwerte (°C)
  int    ampel_gruen_min  = 15;
  int    ampel_gruen_max  = 19;
  int    ampel_gelb_min   = 20;
  int    ampel_gelb_max   = 24;
  int    ampel_rot_min    = 25;
  int    ampel_rot_max    = 99;
  String bio_zone = "A";        // DWD Biowetter-Zone A–J
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

// WMS-Bounding-Box (EPSG:4326: minLon,minLat,maxLon,maxLat) pro DWD-Region
struct DwdBbox { int id; float minLon, minLat, maxLon, maxLat; };
const DwdBbox DWD_BBOX[] = {
  {10,  7.8f, 53.4f, 11.4f, 55.1f},  // Schleswig-Holstein + Hamburg
  {20, 10.6f, 53.1f, 14.4f, 54.7f},  // Mecklenburg-Vorpommern
  {30,  6.6f, 51.3f, 11.6f, 53.9f},  // Niedersachsen + Bremen
  {31, 11.3f, 51.4f, 14.8f, 53.6f},  // Brandenburg + Berlin
  {32, 10.6f, 51.0f, 13.2f, 53.1f},  // Sachsen-Anhalt
  {33,  9.9f, 50.2f, 12.7f, 51.6f},  // Thüringen
  {34, 11.9f, 50.2f, 15.1f, 51.7f},  // Sachsen
  {40,  5.9f, 50.3f,  9.5f, 52.5f},  // Nordrhein-Westfalen
  {50,  7.8f, 49.4f, 10.3f, 51.7f},  // Hessen
  {60,  6.1f, 48.9f,  8.5f, 50.9f},  // Rheinland-Pfalz + Saarland
  {71,  9.0f, 47.3f, 13.9f, 50.6f},  // Bayern (alle Subregionen → gesamt)
  {72,  9.0f, 47.3f, 13.9f, 50.6f},
  {73,  9.0f, 47.3f, 13.9f, 50.6f},
  {74,  9.0f, 47.3f, 13.9f, 50.6f},
  {80,  7.5f, 47.5f, 10.5f, 49.8f},  // Baden-Württemberg
  {0, 0, 0, 0, 0}
};

// Gibt BBOX für cfg.dwd_region zurück; Fallback: Deutschland gesamt
static const DwdBbox* getDwdBbox() {
  for (int i = 0; DWD_BBOX[i].id != 0; i++)
    if (DWD_BBOX[i].id == cfg.dwd_region) return &DWD_BBOX[i];
  static const DwdBbox germany = {0, 5.9f, 47.3f, 15.1f, 55.1f};
  return &germany;
}

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
  float  temp_min    = 0.0f;
  float  temp_max    = 0.0f;
  bool   is_day      = true;
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
  // Open-Meteo Air Quality (Stundenwerte – nächste Stunde, für Screen 1 + Warnung)
  float birke    = -1.0f;
  float graeser  = -1.0f;
  float erle     = -1.0f;
  float beifuss  = -1.0f;
  float ambrosia = -1.0f;
  // Open-Meteo Stunden-Forecast (3 Slots: h+1, h+2, h+3) für ScreenForecastPollenHour
  // [0]=nächste Stunde [1]=übernächste [2]=3. Stunde
  float h_birke[3]    = {-1,-1,-1};
  float h_graeser[3]  = {-1,-1,-1};
  float h_erle[3]     = {-1,-1,-1};
  float h_beifuss[3]  = {-1,-1,-1};
  float h_ambrosia[3] = {-1,-1,-1};
  float h_roggen[3]   = {-1,-1,-1};
  float h_esche[3]    = {-1,-1,-1};
  float h_hasel[3]    = {-1,-1,-1};
  int   h_slot[3]     = {0,0,0};   // Stunde des jeweiligen Slots (0-23)
  // Luftqualität (Air Quality Index + Einzelwerte) – aktuelle + nächste Stunde
  int   aqi      = -1;
  int   aqi_next = -1;
  float pm25     = -1.0f;  float pm25_next = -1.0f;
  float pm10     = -1.0f;  float pm10_next = -1.0f;
  float no2      = -1.0f;  float no2_next  = -1.0f;
  float o3       = -1.0f;  float o3_next   = -1.0f;
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

// Biowetter – 4 Zeitperioden × 7 Kategorien
// Perioden: 0=heute Nmttg, 1=morgen Vmttg, 2=morgen Nmttg, 3=übermorgen Vmttg
// Kategorien: 0=Befinden 1=BP-niedrig 2=BP-hoch 3=Rheuma-entz 4=Rheuma-degen 5=Asthma 6=Wärme
// Werte: 0=kein 1=positiv 2=gering 3=hoch/stark
struct BioWetterDaten {
  int wert[4][7];
  bool geladen = false;
  BioWetterDaten() { memset(wert, 0, sizeof(wert)); }
};

WetterDaten    wetter;
PollenDaten    pollen;
BioWetterDaten bio;
WarnDaten      warnungen[5];
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

static lv_obj_t*  dwdWarnBtn    = nullptr;
static lv_timer_t* dwdBlinker   = nullptr;

unsigned long letztesDatenUpdate  = 0;
unsigned long letztesPollenUpdate = 0;
unsigned long letztesWarnUpdate   = 0;
unsigned long letztesBioUpdate    = 0;
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
      if (dimmed && !cfg.night_mode) {
        // Im Nachtmodus kein Aufhellen beim Touch – Loop regelt Helligkeit
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
  cfg.dim_timeout       = prefs.getInt   ("dim_time",    3);
  cfg.brightness        = prefs.getInt   ("bright",      80);
  cfg.warn_region       = prefs.getBool  ("warn_reg",    true);
  cfg.night_mode        = prefs.getBool  ("night_mode",  false);
  cfg.night_from        = prefs.getInt   ("night_from",  22 * 60);
  cfg.night_to          = prefs.getInt   ("night_to",     6 * 60);
  cfg.night_brightness  = prefs.getInt   ("night_bright", 10);
  cfg.ampel_gruen_min   = prefs.getInt   ("amp_gn_min",  15);
  cfg.ampel_gruen_max   = prefs.getInt   ("amp_gn_max",  19);
  cfg.ampel_gelb_min    = prefs.getInt   ("amp_ge_min",  20);
  cfg.ampel_gelb_max    = prefs.getInt   ("amp_ge_max",  24);
  cfg.ampel_rot_min     = prefs.getInt   ("amp_ro_min",  25);
  cfg.ampel_rot_max     = prefs.getInt   ("amp_ro_max",  99);
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
  prefs.putInt   ("dim_time",    cfg.dim_timeout);
  prefs.putInt   ("bright",      cfg.brightness);
  prefs.putBool  ("warn_reg",    cfg.warn_region);
  prefs.putBool  ("night_mode",  cfg.night_mode);
  prefs.putInt   ("night_from",  cfg.night_from);
  prefs.putInt   ("night_to",    cfg.night_to);
  prefs.putInt   ("night_bright",cfg.night_brightness);
  prefs.putInt   ("amp_gn_min",  cfg.ampel_gruen_min);
  prefs.putInt   ("amp_gn_max",  cfg.ampel_gruen_max);
  prefs.putInt   ("amp_ge_min",  cfg.ampel_gelb_min);
  prefs.putInt   ("amp_ge_max",  cfg.ampel_gelb_max);
  prefs.putInt   ("amp_ro_min",  cfg.ampel_rot_min);
  prefs.putInt   ("amp_ro_max",  cfg.ampel_rot_max);
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
void handleApiAmpel();
void handleApiAmpelSave();
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
  html.replace("%NM%",    cfg.night_mode ? "checked" : "");
  html.replace("%NBR%",   String(cfg.night_brightness));
  // Nacht-Von/Bis Dropdowns (96 Einträge je, 15-Min-Schritte)
  {
    String optFrom, optTo;
    for (int m = 0; m < 24 * 60; m += 15) {
      int hh = m / 60, mm = m % 60;
      char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
      String sel_f = (m == cfg.night_from) ? " selected" : "";
      String sel_t = (m == cfg.night_to)   ? " selected" : "";
      optFrom += "<option value='" + String(m) + "'" + sel_f + ">" + buf + "</option>";
      optTo   += "<option value='" + String(m) + "'" + sel_t + ">" + buf + "</option>";
    }
    html.replace("%NIGHT_FROM_OPTIONS%", optFrom);
    html.replace("%NIGHT_TO_OPTIONS%",   optTo);
  }
  html.replace("%AMP_GN_MIN%", String(cfg.ampel_gruen_min));
  html.replace("%AMP_GN_MAX%", String(cfg.ampel_gruen_max));
  html.replace("%AMP_GE_MIN%", String(cfg.ampel_gelb_min));
  html.replace("%AMP_GE_MAX%", String(cfg.ampel_gelb_max));
  html.replace("%AMP_RO_MIN%", String(cfg.ampel_rot_min));
  html.replace("%AMP_RO_MAX%", String(cfg.ampel_rot_max));
  server.send(200, "text/html", html);
}

void handleWebSave() {
  String prevLocation = cfg.location;
  if (server.hasArg("location"))   cfg.location        = server.arg("location");
  if (server.hasArg("dwd_region")) cfg.dwd_region      = server.arg("dwd_region").toInt();
  cfg.regen_warn      = server.hasArg("regen_warn");
  cfg.pollen_warn     = server.hasArg("pollen_warn");
  cfg.warn_region     = server.hasArg("warn_region");
  cfg.night_mode      = server.hasArg("night_mode");
  if (server.hasArg("night_from"))   cfg.night_from       = server.arg("night_from").toInt();
  if (server.hasArg("night_to"))     cfg.night_to         = server.arg("night_to").toInt();
  if (server.hasArg("night_bright")) cfg.night_brightness = server.arg("night_bright").toInt();
  if (server.hasArg("pol_schw"))   cfg.pollen_schwelle = server.arg("pol_schw").toInt();
  if (server.hasArg("brightness")) cfg.brightness      = server.arg("brightness").toInt();
  if (server.hasArg("dim_time"))   cfg.dim_timeout     = server.arg("dim_time").toInt();
  // Geocoding nur neu anstoßen wenn Location tatsächlich geändert
  if (cfg.location != prevLocation) { cfg.lat = 0.0f; cfg.lon = 0.0f; }
  speichereCfg();
  setBrightness(cfg.brightness);
  server.sendHeader("Location", "/"); server.send(302);
}

void handleApiAmpel() {
  float t = wetter.temp;
  const char* active = "none";
  if      (t >= cfg.ampel_rot_min)   active = "red";
  else if (t >= cfg.ampel_gelb_min)  active = "yellow";
  else if (t >= cfg.ampel_gruen_min) active = "green";

  bool dwd_warn = (anzahl_warnungen > 0 && !dwdWarnBestaetigt);

  String json = "{";
  json += "\"temperature\":"  + String(t, 1) + ",";
  json += "\"dwd_warning\":"  + String(dwd_warn ? "true" : "false") + ",";
  json += "\"active\":\""     + String(active) + "\",";
  json += "\"thresholds\":{";
  json += "\"green\":{\"min\":"  + String(cfg.ampel_gruen_min) + ",\"max\":" + String(cfg.ampel_gruen_max) + "},";
  json += "\"yellow\":{\"min\":" + String(cfg.ampel_gelb_min)  + ",\"max\":" + String(cfg.ampel_gelb_max)  + "},";
  json += "\"red\":{\"min\":"    + String(cfg.ampel_rot_min)   + ",\"max\":" + String(cfg.ampel_rot_max)   + "}";
  json += "}}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiAmpelSave() {
  if (server.hasArg("amp_gn_min")) cfg.ampel_gruen_min = server.arg("amp_gn_min").toInt();
  if (server.hasArg("amp_gn_max")) cfg.ampel_gruen_max = server.arg("amp_gn_max").toInt();
  if (server.hasArg("amp_ge_min")) cfg.ampel_gelb_min  = server.arg("amp_ge_min").toInt();
  if (server.hasArg("amp_ge_max")) cfg.ampel_gelb_max  = server.arg("amp_ge_max").toInt();
  if (server.hasArg("amp_ro_min")) cfg.ampel_rot_min   = server.arg("amp_ro_min").toInt();
  if (server.hasArg("amp_ro_max")) cfg.ampel_rot_max   = server.arg("amp_ro_max").toInt();
  speichereCfg();
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
  server.on("/",               HTTP_GET,  handleWebRoot);
  server.on("/speichern",      HTTP_POST, handleWebSave);
  server.on("/ota_check",      HTTP_GET,  handleWebOtaCheck);
  server.on("/ota_update",     HTTP_POST, handleWebOtaDoUpdate);
  server.on("/wlan",           HTTP_GET,  handleWebWlanAendern);
  server.on("/wlan_speichern", HTTP_POST, handleWebWlanSave);
  server.on("/api/ampel",      HTTP_GET,  handleApiAmpel);
  server.on("/api/ampel_save", HTTP_POST, handleApiAmpelSave);
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

// Konvertiert Open-Meteo Pollenwerte (Körner/m³) auf DWD 0–3 Skala
// Schwellwerte identisch mit WetterCube setPollenLabel()
float openMeteoToDwd(float grains) {
  if (grains < 0)    return -1.0f;
  if (grains == 0)   return  0.0f;
  if (grains <= 10)  return  1.0f;   // gering
  if (grains <= 30)  return  2.0f;   // mittel
  if (grains <= 100) return  2.5f;   // hoch
  return                     3.0f;   // stark
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

const lv_img_dsc_t* wmoZuImage(int wmo, bool day) {
  if (wmo == 0)   return day ? &day_clear             : &night_full_moon_clear;          // Klar
  if (wmo <= 2)   return day ? &day_partial_cloud     : &night_full_moon_partial_cloud;  // Teilweise bewölkt
  if (wmo == 3)   return &overcast;                                                      // Bedeckt
  if (wmo <= 48)  return &fog;                                                           // Nebel
  if (wmo <= 57)  return &rain;                                                          // Nieselregen
  if (wmo <= 67)  return &sleet;                                                         // Gefrierender Regen / Schneeregen
  if (wmo <= 77)  return &snow;                                                          // Schnee
  if (wmo <= 82)  return &rain;                                                          // Regenschauer
  if (wmo <= 86)  return &snow;                                                          // Schneeschauer
  return &thunder;                                                                       // Gewitter
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
  url += ",wind_speed_10m,wind_direction_10m,surface_pressure,weather_code,is_day";
  url += "&daily=uv_index_max,sunrise,sunset,temperature_2m_min,temperature_2m_max";
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
      wetter.is_day     = c["is_day"].as<int>() != 0;
      wetter.uv_index   = doc["daily"]["uv_index_max"][0].as<float>();
      wetter.temp_min   = doc["daily"]["temperature_2m_min"][0].as<float>();
      wetter.temp_max   = doc["daily"]["temperature_2m_max"][0].as<float>();
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
  int httpCode = http.GET();
  Serial.printf("[Pollen] HTTP %d, region=%d\n", httpCode, cfg.dwd_region);
  if (httpCode == 200) {
    StaticJsonDocument<128> filter;
    JsonArray fc = filter.createNestedArray("content");
    JsonObject fe = fc.createNestedObject();
    fe["region_id"] = true; fe["Pollen"] = true;
    DynamicJsonDocument doc(16384);
    String body = http.getString();
    Serial.printf("[Pollen] Body %u Bytes\n", body.length());
    DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    Serial.printf("[Pollen] JSON: %s\n", err.c_str());
    if (err == DeserializationError::Ok) {
      bool found = false;
      for (JsonObject region : doc["content"].as<JsonArray>()) {
        if (region["region_id"].as<int>() != cfg.dwd_region) continue;
        found = true;
        JsonObject p = region["Pollen"];
        Serial.printf("[Pollen] Graeser=%.1f Birke=%.1f\n",
          parseDwdWert(p["Graeser"]["today"]|""), parseDwdWert(p["Birke"]["today"]|""));
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
      if (!found) Serial.printf("[Pollen] Region %d nicht gefunden!\n", cfg.dwd_region);
    }
  }
  http.end();
}

// Mappt DWD Pollen-Region auf 2-stellige AGS-Bundesland-Prefixes (AGS-Schlüssel in warnings.json)
static const char* agsPrefix(int region) {
  switch (region) {
    case 10: return "01,02";      // Schleswig-Holstein, Hamburg
    case 20: return "13";         // Mecklenburg-Vorpommern
    case 30: return "03,04";      // Niedersachsen, Bremen
    case 31: return "11,12";      // Berlin, Brandenburg
    case 32: return "15";         // Sachsen-Anhalt
    case 33: return "16";         // Thüringen
    case 34: return "14";         // Sachsen
    case 40: return "05";         // Nordrhein-Westfalen
    case 50: return "06";         // Hessen
    case 60: return "07,10";      // Rheinland-Pfalz, Saarland
    case 71: case 72: case 73: case 74: return "09"; // Bayern
    case 80: return "08";         // Baden-Württemberg
    default: return "";
  }
}

static bool agsMatchesRegion(const char* ags, int region) {
  const char* prefixes = agsPrefix(region);
  if (!prefixes || !prefixes[0]) return true; // kein Filter → alle
  // prefixes ist kommagetrennt, z.B. "01,02"
  while (*prefixes) {
    if (strncmp(ags, prefixes, 2) == 0) return true;
    prefixes += 2;
    if (*prefixes == ',') prefixes++;
  }
  return false;
}

// -- DWD Wetterwarnungen via GeoServer WFS mit BBOX-Filter --
void fetchDwdWarnungen() {
  if (WiFi.status() != WL_CONNECTED || !cfg.warn_region) return;
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;

  const DwdBbox* bb = getDwdBbox();
  // WFS BBOX-Filter: nur Warnungen im Bundesland-Ausschnitt (keine Feldnamen nötig)
  // BBOX-Format für WFS 1.1.0: minLon,minLat,maxLon,maxLat,EPSG:4326
  String url = "https://maps.dwd.de/geoserver/dwd/ows?service=WFS&version=1.1.0"
               "&request=GetFeature&typeName=dwd:Warnungen_Gemeinden"
               "&outputFormat=application/json&maxFeatures=20";
  url += "&BBOX=" + String(bb->minLon,4) + "," + String(bb->minLat,4) + ","
               + String(bb->maxLon,4) + "," + String(bb->maxLat,4) + ",EPSG:4326";

  Serial.printf("[DWD-Warn] URL: %s\n", url.c_str());
  http.begin(sc, url);
  http.addHeader("User-Agent", "WetterCubePlus/1.0");
  http.setTimeout(15000);
  int code = http.GET();
  anzahl_warnungen = 0;
  Serial.printf("[DWD-Warn] HTTP %d\n", code);

  if (code == 200) {
    String body = http.getString();
    http.end();
    Serial.printf("[DWD-Warn] %u Bytes\n", body.length());
    // GeoJSON FeatureCollection – Feldnamen aus properties loggen um sie zu kennen
    DynamicJsonDocument doc(32768);
    DeserializationError err = deserializeJson(doc, body);
    body = String();
    Serial.printf("[DWD-Warn] parse: %s\n", err.c_str());
    if (err == DeserializationError::Ok || err == DeserializationError::NoMemory) {
      int count = 0;
      for (JsonObject feat : doc["features"].as<JsonArray>()) {
        if (count >= 5) break;
        JsonObject p = feat["properties"];

        // Feldnamen beim ersten Feature ins Serial schreiben (Diagnose)
        if (count == 0) {
          Serial.print("[DWD-Warn] Felder: ");
          for (JsonPair kv : p) { Serial.print(kv.key().c_str()); Serial.print(" "); }
          Serial.println();
        }

        // Severity – verschiedene mögliche Feldnamen probieren
        const char* sevStr = p["SEVERITY"] | p["severity"] | p["Severity"] | "Minor";
        int sev = 1;
        if      (strcmp(sevStr, "Extreme")  == 0) sev = 4;
        else if (strcmp(sevStr, "Severe")   == 0) sev = 3;
        else if (strcmp(sevStr, "Moderate") == 0) sev = 2;

        // Titel / Headline
        const char* titelC = p["HEADLINE"] | p["headline"] | p["Headline"] | p["NAME"] | "";
        String titel = titelC;
        bool dup = false;
        for (int i = 0; i < count; i++) if (warnungen[i].titel == titel) { dup = true; break; }
        if (dup) continue;

        warnungen[count].aktiv        = true;
        warnungen[count].level        = sev;
        warnungen[count].typ          = String(p["EVENT"]       | p["event"]       | p["Event"]       | "");
        warnungen[count].titel        = titel;
        warnungen[count].beschreibung = String(p["DESCRIPTION"] | p["description"] | p["Description"] | "");
        Serial.printf("[DWD-Warn] #%d sev=%d typ=%s titel=%s\n",
          count, sev, warnungen[count].typ.c_str(), warnungen[count].titel.substring(0,40).c_str());
        count++;
      }
      anzahl_warnungen = count;
    }
  } else {
    http.end();
  }
  Serial.printf("[DWD-Warn] %d Warnungen fuer Region %d\n", anzahl_warnungen, cfg.dwd_region);
}

// -- DWD WMS Niederschlagsradar (PNG → RGB565A8 im PSRAM, bundeslandbezogen) --
void fetchWmsRadar() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;

  const DwdBbox* bb = getDwdBbox();
  String url = "https://maps.dwd.de/geoserver/dwd/ows?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap";
  url += "&LAYERS=dwd:KV_VG250_BUNDESLAENDER_2020,dwd:Niederschlagsradar";
  url += "&SRS=EPSG:4326";
  url += "&WIDTH=480&HEIGHT=270";
  url += "&FORMAT=image/png";
  url += "&BBOX=";
  url += String(bb->minLon, 4) + "," + String(bb->minLat, 4) + ",";
  url += String(bb->maxLon, 4) + "," + String(bb->maxLat, 4);
  Serial.printf("[Radar] URL: %s\n", url.c_str());
  http.begin(sc, url);
  http.addHeader("User-Agent", "WetterCubePlus/1.0");
  http.setTimeout(30000);
  const char* hdrs[] = {"Content-Type"};
  http.collectHeaders(hdrs, 1);
  int httpCode = http.GET();
  Serial.printf("[Radar] HTTP %d\n", httpCode);
  if (httpCode != 200) { http.end(); return; }
  unsigned long tStart = millis();
  String body = http.getString();
  http.end();
  size_t got = body.length();
  Serial.printf("[Radar] Download: %u Bytes in %lums\n", got, millis() - tStart);

  String ct = http.header("Content-Type");
  if (!ct.startsWith("image/png")) {
    Serial.printf("[Radar] Fehler (mit Grenzen): %s\n", body.substring(0, 200).c_str());
    // Fallback: nur Radar-Layer ohne Bundesland-Grenzen
    WiFiClientSecure sc2; sc2.setInsecure();
    HTTPClient http2;
    String url2 = "https://maps.dwd.de/geoserver/dwd/ows?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap";
    url2 += "&LAYERS=dwd:Niederschlagsradar";
    url2 += "&SRS=EPSG:4326&WIDTH=480&HEIGHT=270&FORMAT=image/png";
    url2 += "&BBOX=" + String(bb->minLon,4) + "," + String(bb->minLat,4) + ","
                     + String(bb->maxLon,4) + "," + String(bb->maxLat,4);
    Serial.printf("[Radar] Fallback-URL: %s\n", url2.c_str());
    http2.begin(sc2, url2);
    http2.addHeader("User-Agent", "WetterCubePlus/1.0");
    http2.setTimeout(30000);
    const char* hdrs2[] = {"Content-Type"};
    http2.collectHeaders(hdrs2, 1);
    int c2 = http2.GET();
    if (c2 != 200) { http2.end(); return; }
    body = http2.getString();
    ct   = http2.header("Content-Type");
    http2.end();
    got  = body.length();
    Serial.printf("[Radar] Fallback: %u Bytes, CT: %s\n", got, ct.c_str());
    if (!ct.startsWith("image/png")) { Serial.println("[Radar] Fallback fehlgeschlagen"); return; }
  }
  if (got < 512) { Serial.printf("[Radar] Zu wenig Daten: %u\n", got); return; }
  const uint8_t* pngBuf = (const uint8_t*)body.c_str();
  Serial.printf("[Radar] %u Bytes geladen, dekodiere PNG\n", got);

  // PNG → RGBA8888 (lodepng nutzt malloc; mit PSRAM als Heap-Erweiterung reicht der Speicher)
  unsigned char* rgba = nullptr;
  unsigned imgW = 0, imgH = 0;
  unsigned err = lodepng_decode32(&rgba, &imgW, &imgH, pngBuf, got);

  if (err || !rgba) {
    Serial.printf("[Radar] lodepng Fehler %u\n", err);
    if (rgba) free(rgba);
    return;
  }
  Serial.printf("[Radar] Dekodiert: %ux%u\n", imgW, imgH);

  // RGBA8888 → RGB565A8 im PSRAM (Format: [RGB565 block][Alpha block])
  // Mit LV_COLOR_16_SWAP=1: Bytes im RGB565-Word tauschen
  size_t rgb565Size  = (size_t)imgW * imgH * 2;
  size_t alphaSize   = (size_t)imgW * imgH;
  size_t totalSize   = rgb565Size + alphaSize;
  uint8_t* decoded = (uint8_t*)ps_malloc(totalSize);
  if (!decoded) {
    Serial.println("[Radar] ps_malloc decoded fehlgeschlagen");
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
    lv_img_set_zoom(imgWarnkarte, 256);  // 1:1, WMS liefert 480x280 passend
    lv_img_set_antialias(imgWarnkarte, false);
    lv_obj_align(imgWarnkarte, LV_ALIGN_TOP_MID, 0, 0);
    if (lblWarnkarteHint) lv_obj_add_flag(lblWarnkarteHint, LV_OBJ_FLAG_HIDDEN);
    aktualisiereRadarMarker();
    Serial.printf("[Radar] Angezeigt %ux%u\n", imgW, imgH);
  }
}

static lv_obj_t* radarMarker  = nullptr;
static lv_obj_t* windBg       = nullptr;   // dunkler Hintergrundkreis für Pfeil
static lv_obj_t* windShaft    = nullptr;   // Pfeilschaft
static lv_obj_t* windHead1    = nullptr;   // Pfeilspitze links
static lv_obj_t* windHead2    = nullptr;   // Pfeilspitze rechts
static lv_point_t wPts0[2], wPts1[2], wPts2[2];  // Punkte-Arrays (müssen static bleiben)

void erstelleWarnkarteScreen() {
  if (!objects.screenwarnkarte1) return;
  // Hint zuerst, Bild danach → Bild liegt im Z-Order oben
  lblWarnkarteHint = lv_label_create(objects.screenwarnkarte1);
  if (lblWarnkarteHint) {
    lv_label_set_text(lblWarnkarteHint, "Lade Radarkarte ...");
    lv_obj_align(lblWarnkarteHint, LV_ALIGN_CENTER, 0, 0);
  }
  imgWarnkarte = lv_img_create(objects.screenwarnkarte1);
  if (imgWarnkarte) {
    lv_obj_align(imgWarnkarte, LV_ALIGN_TOP_MID, 0, 0);
  }
  // Standort-Marker (roter Punkt)
  radarMarker = lv_obj_create(objects.screenwarnkarte1);
  if (radarMarker) {
    lv_obj_set_size(radarMarker, 10, 10);
    lv_obj_set_style_radius(radarMarker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(radarMarker, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_bg_opa(radarMarker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(radarMarker, 0, 0);
    lv_obj_clear_flag(radarMarker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(radarMarker, LV_OBJ_FLAG_HIDDEN);
  }
  // Windpfeil – dunkler Hintergrundkreis (50×50) oben rechts
  windBg = lv_obj_create(objects.screenwarnkarte1);
  if (windBg) {
    lv_obj_set_size(windBg, 50, 50);
    lv_obj_set_pos(windBg, 424, 4);
    lv_obj_set_style_radius(windBg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(windBg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(windBg, 90, 0);
    lv_obj_set_style_border_width(windBg, 0, 0);
    lv_obj_clear_flag(windBg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(windBg, LV_OBJ_FLAG_HIDDEN);
  }
  // Pfeillinien (3 Segmente: Schaft + 2 Pfeilspitzen)
  auto mkLine = [](lv_obj_t*& l, lv_obj_t* parent) {
    l = lv_line_create(parent);
    lv_obj_set_style_line_color(l, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_line_width(l, 2, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
  };
  mkLine(windShaft, objects.screenwarnkarte1);
  mkLine(windHead1, objects.screenwarnkarte1);
  mkLine(windHead2, objects.screenwarnkarte1);
}

void aktualisiereRadarMarker() {
  if (!radarMarker || cfg.lat == 0.0f) return;
  const DwdBbox* bb = getDwdBbox();
  float px = (cfg.lon - bb->minLon) / (bb->maxLon - bb->minLon) * 480.0f;
  float py = (1.0f - (cfg.lat - bb->minLat) / (bb->maxLat - bb->minLat)) * 270.0f;
  int ix = (int)px - 5;
  int iy = (int)py - 5;
  lv_obj_set_pos(radarMarker, ix, iy);
  lv_obj_clear_flag(radarMarker, LV_OBJ_FLAG_HIDDEN);
  Serial.printf("[Radar] Marker: lat=%.4f lon=%.4f → px=%d py=%d\n", cfg.lat, cfg.lon, ix+5, iy+5);

  // Windpfeil: wind_dir gibt an, woher der Wind kommt → +180° = Zugrichtung
  if (!windShaft) return;
  const int CX = 449, CY = 29;   // Mitte des 50×50 Kreises (pos 424+25, 4+25)
  const int LEN = 16, HEAD = 7;
  float rad = fmod(wetter.wind_dir + 180.0f, 360.0f) * M_PI / 180.0f;
  float dx = sinf(rad), dy = -cosf(rad);
  int tx = CX + (int)(dx * LEN);  // Pfeilspitze
  int ty = CY + (int)(dy * LEN);
  int bx = CX - (int)(dx * LEN);  // Pfeilende
  int by = CY - (int)(dy * LEN);
  // Schaft
  wPts0[0] = {(lv_coord_t)bx, (lv_coord_t)by};
  wPts0[1] = {(lv_coord_t)tx, (lv_coord_t)ty};
  lv_line_set_points(windShaft, wPts0, 2);
  // Pfeilspitzen (±140° von Zugrichtung)
  float h1 = rad + 2.44f, h2 = rad - 2.44f;
  wPts1[0] = {(lv_coord_t)tx, (lv_coord_t)ty};
  wPts1[1] = {(lv_coord_t)(tx + (int)(sinf(h1)*HEAD)), (lv_coord_t)(ty - (int)(cosf(h1)*HEAD))};
  wPts2[0] = {(lv_coord_t)tx, (lv_coord_t)ty};
  wPts2[1] = {(lv_coord_t)(tx + (int)(sinf(h2)*HEAD)), (lv_coord_t)(ty - (int)(cosf(h2)*HEAD))};
  lv_line_set_points(windHead1, wPts1, 2);
  lv_line_set_points(windHead2, wPts2, 2);
  lv_obj_clear_flag(windBg,    LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(windShaft, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(windHead1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(windHead2, LV_OBJ_FLAG_HIDDEN);
  Serial.printf("[Radar] Windpfeil: dir=%.0f° → Zug=%.0f°\n", wetter.wind_dir, fmod(wetter.wind_dir+180.0f,360.0f));
}

// Forward-Deklarationen (definiert weiter unten)
static String ohneUmlaute(const String& s);
static lv_obj_t* warnPopup = nullptr;
static bool warnPopupIstRegen = false;
static bool warnPopupNurSchliessen = false;
static void zeigeWarnPopup(lv_obj_t* parent, bool istRegen, const char* titel, const char* text, bool nurSchliessen = false);

// -- DWD Warnliste (ScreenWarnkarte2) --
static lv_obj_t* warnListeContainer = nullptr;

static lv_color_t dwdSevColor(int sev) {
  switch (sev) {
    case 4: return lv_color_hex(0xC000C0);
    case 3: return lv_color_hex(0xFF0000);
    case 2: return lv_color_hex(0xFF8000);
    default:return lv_color_hex(0xFFCC00);
  }
}
static bool dwdSevDarkText(int sev) { return sev <= 1; }

// Ersetzt deutsche Umlaute durch ASCII-Äquivalente (Montserrat-Font hat keine)
static String ohneUmlaute(const String& s) {
  String r = "";
  const uint8_t* p = (const uint8_t*)s.c_str();
  while (*p) {
    if (p[0] == 0xC3) { // UTF-8 2-Byte-Sequenz
      switch (p[1]) {
        case 0xA4: r += "ae"; p+=2; continue; // ä
        case 0xB6: r += "oe"; p+=2; continue; // ö
        case 0xBC: r += "ue"; p+=2; continue; // ü
        case 0x84: r += "Ae"; p+=2; continue; // Ä
        case 0x96: r += "Oe"; p+=2; continue; // Ö
        case 0x9C: r += "Ue"; p+=2; continue; // Ü
        case 0x9F: r += "ss"; p+=2; continue; // ß
        default: break;
      }
    }
    // Latin-1 Umlaute (falls nicht UTF-8)
    uint8_t c = *p;
    if      (c == 0xE4) { r += "ae"; p++; continue; }
    else if (c == 0xF6) { r += "oe"; p++; continue; }
    else if (c == 0xFC) { r += "ue"; p++; continue; }
    else if (c == 0xC4) { r += "Ae"; p++; continue; }
    else if (c == 0xD6) { r += "Oe"; p++; continue; }
    else if (c == 0xDC) { r += "Ue"; p++; continue; }
    else if (c == 0xDF) { r += "ss"; p++; continue; }
    r += (char)c; p++;
  }
  return r;
}

static void cbWarnKarteCardTap(lv_event_t* e) {
  if (warnPopup) { lv_obj_del(warnPopup); warnPopup = nullptr; return; }
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= anzahl_warnungen) return;
  const char* levelName[] = {"", "Vorinformation", "Warnung", "Schwere Warnung", "Extreme Warnung"};
  int lv = constrain(warnungen[idx].level, 0, 4);
  String text = String(levelName[lv]) + "\n" + ohneUmlaute(warnungen[idx].beschreibung).substring(0, 200);
  String titel = ohneUmlaute(warnungen[idx].titel);
  zeigeWarnPopup(objects.screenwarnkarte2, false, titel.c_str(), text.c_str(), true);
}

void aktualisiereWarnliste() {
  if (!objects.screenwarnkarte2) return;
  if (warnListeContainer) { lv_obj_del(warnListeContainer); warnListeContainer = nullptr; }

  warnListeContainer = lv_obj_create(objects.screenwarnkarte2);
  lv_obj_set_size(warnListeContainer, 480, 270);
  lv_obj_align(warnListeContainer, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(warnListeContainer, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(warnListeContainer, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(warnListeContainer, 0, 0);
  lv_obj_set_style_pad_all(warnListeContainer, 4, 0);
  // Scrollbar-Stil
  lv_obj_set_style_bg_color(warnListeContainer, lv_color_hex(0x444444), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(warnListeContainer, LV_OPA_COVER, LV_PART_SCROLLBAR);

  if (anzahl_warnungen == 0) {
    lv_obj_t* lbl = lv_label_create(warnListeContainer);
    lv_label_set_text(lbl, "Keine aktiven\nDWD-Wetterwarnungen");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(warnListeContainer, LV_OBJ_FLAG_SCROLLABLE);
    return;
  }

  const int CARD_H = 120; // feste Kartenhöhe → scrollbar wenn >2 Warnungen
  int totalH = anzahl_warnungen * (CARD_H + 4) + 4;
  lv_obj_set_height(warnListeContainer, 270);
  // Innerhalb des Containers: flexibler Inhalt durch manuelles Positionieren

  for (int i = 0; i < anzahl_warnungen && i < 5; i++) {
    lv_color_t col = dwdSevColor(warnungen[i].level);
    bool dark = dwdSevDarkText(warnungen[i].level);
    lv_color_t textCol = dark ? lv_color_hex(0x111111) : lv_color_hex(0xffffff);

    lv_obj_t* card = lv_obj_create(warnListeContainer);
    lv_obj_set_size(card, 466, CARD_H);
    lv_obj_set_pos(card, 0, i * (CARD_H + 4));
    lv_obj_set_style_bg_color(card, col, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, cbWarnKarteCardTap, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    lv_obj_t* lblTyp = lv_label_create(card);
    lv_label_set_text(lblTyp, ohneUmlaute(warnungen[i].typ).c_str());
    lv_obj_set_style_text_font(lblTyp, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTyp, textCol, 0);
    lv_obj_align(lblTyp, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* lblTitel = lv_label_create(card);
    lv_label_set_long_mode(lblTitel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lblTitel, 454);
    lv_label_set_text(lblTitel, ohneUmlaute(warnungen[i].titel).c_str());
    lv_obj_set_style_text_font(lblTitel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblTitel, textCol, 0);
    lv_obj_align(lblTitel, LV_ALIGN_TOP_LEFT, 0, 15);

    lv_obj_t* lblDesc = lv_label_create(card);
    lv_label_set_long_mode(lblDesc, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lblDesc, 454);
    lv_label_set_text(lblDesc, ohneUmlaute(warnungen[i].beschreibung).substring(0, 100).c_str());
    lv_obj_set_style_text_font(lblDesc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblDesc, textCol, 0);
    lv_obj_align(lblDesc, LV_ALIGN_TOP_LEFT, 0, 54);

    // Hinweis: tippen für Details
    lv_obj_t* lblHint = lv_label_create(card);
    lv_label_set_text(lblHint, ">> Tippen fuer Details");
    lv_obj_set_style_text_font(lblHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblHint, dark ? lv_color_hex(0x333333) : lv_color_hex(0xdddddd), 0);
    lv_obj_align(lblHint, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  }

  // Scrollbar aktivieren wenn Inhalt > Sichtbereich
  if (totalH > 270) {
    lv_obj_add_flag(warnListeContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(warnListeContainer, LV_DIR_VER);
  } else {
    lv_obj_clear_flag(warnListeContainer, LV_OBJ_FLAG_SCROLLABLE);
  }
}

void erstelleWarnkarte2Screen() {
  aktualisiereWarnliste();
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
  url += ",european_aqi,pm2_5,pm10,nitrogen_dioxide,ozone";
  url += "&timezone=auto&forecast_days=1";
  http.begin(sc, url);
  if (http.GET() == 200) {
    DynamicJsonDocument doc(10240);
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      struct tm ti; getLocalTime(&ti);
      // Index in das 48h-Array: Stunde 0–23 = heute, 24–47 = morgen
      int h = ti.tm_hour + 1;  // nächste Stunde; läuft bis max 24 (=Index 0 von morgen)
      if (h > 47) h = 47;
      // Nächste Stunde (für Screen 1 + Warnung)
      pollen.birke    = doc["hourly"]["birch_pollen"][h].as<float>();
      pollen.graeser  = doc["hourly"]["grass_pollen"][h].as<float>();
      pollen.erle     = doc["hourly"]["alder_pollen"][h].as<float>();
      pollen.beifuss  = doc["hourly"]["mugwort_pollen"][h].as<float>();
      pollen.ambrosia = doc["hourly"]["ragweed_pollen"][h].as<float>();
      // Luftqualität – aktuelle Stunde (Index ti.tm_hour) + nächste Stunde (h)
      int hNow = ti.tm_hour;
      pollen.aqi      = doc["hourly"]["european_aqi"][hNow].as<int>();
      pollen.aqi_next = doc["hourly"]["european_aqi"][h].as<int>();
      pollen.pm25     = doc["hourly"]["pm2_5"][hNow].as<float>();
      pollen.pm25_next= doc["hourly"]["pm2_5"][h].as<float>();
      pollen.pm10     = doc["hourly"]["pm10"][hNow].as<float>();
      pollen.pm10_next= doc["hourly"]["pm10"][h].as<float>();
      pollen.no2      = doc["hourly"]["nitrogen_dioxide"][hNow].as<float>();
      pollen.no2_next = doc["hourly"]["nitrogen_dioxide"][h].as<float>();
      pollen.o3       = doc["hourly"]["ozone"][hNow].as<float>();
      pollen.o3_next  = doc["hourly"]["ozone"][h].as<float>();
      // 3 Stunden-Slots für ScreenForecastPollenHour — Index läuft über Tagesgrenze
      for (int i = 0; i < 3; i++) {
        int slot = min(h + i, 47);
        pollen.h_slot[i]    = slot % 24;  // Anzeige-Stunde: 0–23
        pollen.h_birke[i]   = doc["hourly"]["birch_pollen"][slot].as<float>();
        pollen.h_graeser[i] = doc["hourly"]["grass_pollen"][slot].as<float>();
        pollen.h_erle[i]    = doc["hourly"]["alder_pollen"][slot].as<float>();
        pollen.h_beifuss[i] = doc["hourly"]["mugwort_pollen"][slot].as<float>();
        pollen.h_ambrosia[i]= doc["hourly"]["ragweed_pollen"][slot].as<float>();
        // Roggen, Esche, Hasel: Open-Meteo bietet diese nicht stündlich → DWD-Tageswert als Näherung
        pollen.h_roggen[i]  = pollen.dwd_roggen;
        pollen.h_esche[i]   = pollen.dwd_esche;
        pollen.h_hasel[i]   = pollen.dwd_hasel;
      }
    }
  }
  http.end();
}

// ============================================================
//  DWD Biowetter
// ============================================================

// 0=kein 1=positiv 2=gering 3=hoch/stark
static int bioWertToInt(const char* v) {
  if (!v)                             return 0;
  if (strstr(v, "positiver"))         return 1;
  if (strstr(v, "geringe"))           return 2;
  if (strstr(v, "hohe"))              return 3;
  if (strstr(v, "schwache"))          return 2;  // Wärmebelastung schwach
  if (strstr(v, "mäßige"))            return 3;  // Wärmebelastung mäßig/stark
  return 0;
}

static lv_color_t bioWertColor(int v) {
  switch (v) {
    case 1:  return lv_color_hex(0x50c878);  // positiv – grün
    case 2:  return lv_color_hex(0xffd700);  // geringe Gefährdung – gelb
    case 3:  return lv_color_hex(0xff3030);  // hohe Gefährdung – rot
    default: return lv_color_hex(0x666666);  // kein Einfluss – grau
  }
}

static const char* bioWertKurz(int v) {
  switch (v) {
    case 1:  return "positiv";
    case 2:  return "gering";
    case 3:  return "hoch";
    default: return "kein";
  }
}

void fetchBioWetter() {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[Bio] Kein WiFi"); return; }
  Serial.printf("[Bio] Lade Zone %s...\n", cfg.bio_zone.c_str());
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  http.begin(sc, "https://opendata.dwd.de/climate_environment/health/alerts/biowetter.json");
  http.setTimeout(15000);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[Bio] HTTP %d\n", code);
    http.end(); return;
  }
  int contentLen = http.getSize();
  Serial.printf("[Bio] HTTP 200, %d Bytes\n", contentLen);

  // Ganzen Body lesen (110KB liegt im PSRAM)
  String body = http.getString();
  http.end();
  Serial.printf("[Bio] Body geladen: %d Bytes\n", body.length());
  if (body.length() < 1000) { Serial.println("[Bio] Body zu kurz"); return; }

  const char* perioden[] = {"today_afternoon","tomorrow_morning","tomorrow_afternoon","dayafter_to_morning"};
  DynamicJsonDocument filter(1024);
  {
    JsonArray zf = filter.createNestedArray("zone");
    JsonObject zfe = zf.createNestedObject();
    zfe["id"] = true;
    for (auto p : perioden) {
      JsonObject pf = zfe.createNestedObject(p);
      JsonArray ef = pf.createNestedArray("effect");
      JsonObject efe = ef.createNestedObject();
      efe["value"] = true;
    }
  }

  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err != DeserializationError::Ok) {
    Serial.printf("[Bio] JSON Fehler: %s\n", err.c_str());
    return;
  }

  JsonArray zones = doc["zone"];
  Serial.printf("[Bio] %d Zonen geparst\n", (int)zones.size());
  for (JsonObject z : zones) {
    String id = z["id"].as<String>();
    if (id != cfg.bio_zone) continue;
    for (int p = 0; p < 4; p++) {
      JsonArray eff = z[perioden[p]]["effect"];
      for (int k = 0; k < 7 && k < (int)eff.size(); k++)
        bio.wert[p][k] = bioWertToInt(eff[k]["value"].as<const char*>());
    }
    bio.geladen = true;
    Serial.printf("[Bio] Zone %s: Bef=%d BP-=%d BP+=%d Rh-e=%d Rh-d=%d Ast=%d Wärm=%d\n",
      cfg.bio_zone.c_str(),
      bio.wert[0][0], bio.wert[0][1], bio.wert[0][2],
      bio.wert[0][3], bio.wert[0][4], bio.wert[0][5], bio.wert[0][6]);
    break;
  }
  if (!bio.geladen) Serial.printf("[Bio] Zone %s nicht gefunden\n", cfg.bio_zone.c_str());
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

// Schlimmsten Pollenverursacher (naechste Stunde, Open-Meteo) als String
static String schleimsterPollen() {
  float max_v = -1.0f;
  const char* name = "Pollen";
  struct { float v; const char* n; } list[] = {
    {openMeteoToDwd(pollen.birke),    "Birke"},
    {openMeteoToDwd(pollen.erle),     "Erle"},
    {openMeteoToDwd(pollen.graeser),  "Gräser"},
    {openMeteoToDwd(pollen.beifuss),  "Beifuß"},
    {openMeteoToDwd(pollen.ambrosia), "Ambrosia"},
  };
  for (auto& e : list) { if (e.v > max_v) { max_v = e.v; name = e.n; } }
  return String(name);
}

// Europäischer AQI: 0–20 gut, 20–40 mittel, 40–60 mäßig, 60–80 schlecht, 80–100 sehr schlecht, >100 extrem
static lv_color_t aqiColor(int aqi) {
  if (aqi < 0)   return lv_color_hex(0x888888);
  if (aqi <= 20)  return lv_color_hex(0x50c878);  // gut – grün
  if (aqi <= 40)  return lv_color_hex(0xadff2f);  // mittel – gelbgrün
  if (aqi <= 60)  return lv_color_hex(0xffd700);  // mäßig – gelb
  if (aqi <= 80)  return lv_color_hex(0xff8c00);  // schlecht – orange
  if (aqi <= 100) return lv_color_hex(0xff3030);  // sehr schlecht – rot
  return lv_color_hex(0x8b008b);                  // extrem – violett
}

static const char* aqiStatusText(int aqi) {
  if (aqi < 0)    return "--";
  if (aqi <= 20)  return "Gut";
  if (aqi <= 40)  return "Mittel";
  if (aqi <= 60)  return "Maessig";
  if (aqi <= 80)  return "Schlecht";
  if (aqi <= 100) return "Sehr schlecht";
  return "Extrem";
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
  if (objects.imagewetter) lv_img_set_src(objects.imagewetter, wmoZuImage(wetter.wmo_code, wetter.is_day));

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

  // screen_1: Top-3 Pollen (Open-Meteo, naechste Stunde, auf DWD-Skala umgerechnet)
  {
    struct PE { float v; const char* n; };
    PE pe[] = {
      {openMeteoToDwd(pollen.birke),    "Birke"},
      {openMeteoToDwd(pollen.erle),     "Erle"},
      {openMeteoToDwd(pollen.graeser),  "Gräser"},
      {openMeteoToDwd(pollen.beifuss),  "Beifuß"},
      {openMeteoToDwd(pollen.ambrosia), "Ambrosia"},
    };
    for (int i = 0; i < 4; i++)
      for (int j = i+1; j < 5; j++)
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
      setLabelFmt(htemp[i], tempColor(wetter.temp_forecast[i]), buf);
    }
    if (hws[i]) {
      snprintf(buf, sizeof(buf), "%.0f km/h", wetter.wind_speed_forecast[i]);
      lv_label_set_text(hws[i], buf);
    }
    setLabel(hwd[i], windRichtung(wetter.wind_dir_forecast[i]));
    if (himg[i]) lv_img_set_src(himg[i], wmoZuImage(wetter.wmo_forecast[i], true));
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

  // ── SunMoon (screensunmoon) ─────────────────────────────────
  if (objects.labeluvindexvalue) {
    snprintf(buf, sizeof(buf), "%.0f", wetter.uv_index);
    lv_label_set_text(objects.labeluvindexvalue, buf);
  }
  if (objects.labelsunrisetime)
    lv_label_set_text(objects.labelsunrisetime, wetter.sunrise.isEmpty() ? "--:--" : wetter.sunrise.c_str());
  if (objects.labelsundowntime)
    lv_label_set_text(objects.labelsundowntime, wetter.sunset.isEmpty()  ? "--:--" : wetter.sunset.c_str());
  if (objects.labeltempminvalue) {
    snprintf(buf, sizeof(buf), "%.0f °C", wetter.temp_min);
    setLabelFmt(objects.labeltempminvalue, tempColor(wetter.temp_min), buf);
  }
  if (objects.labeltempmaxvalue) {
    snprintf(buf, sizeof(buf), "%.0f °C", wetter.temp_max);
    setLabelFmt(objects.labeltempmaxvalue, tempColor(wetter.temp_max), buf);
  }

  // ── ScreenForecastPollenHour (stündliche Pollen, 3 Slots) ───
  {
    char hbuf[6];
    lv_obj_t* timeLabels[3]   = {objects.labelhour1,   objects.labelhour2,   objects.labelhour3};
    lv_obj_t* birke[3]        = {objects.labelbirkehour1,   objects.labelbirkehour2,   objects.labelbirkehour3};
    lv_obj_t* graeser[3]      = {objects.labelgraeserhour1, objects.labelgraeserhour2, objects.labelgraeserhour3};
    lv_obj_t* erle[3]         = {objects.labelerlehour1,    objects.labelerlehour2,    objects.labelerlehour3};
    lv_obj_t* hasel[3]        = {objects.labelhaselhour1,   objects.labelhaselhour2,   objects.labelhaselhour3};
    lv_obj_t* beifuss[3]      = {objects.labelbeifusshour1, objects.labelbeifusshour2, objects.labelbeifusshour3};
    lv_obj_t* ambrosia[3]     = {objects.labelambrosiahour1,objects.labelambrosiahour2,objects.labelambrosiahour3};
    lv_obj_t* roggen[3]       = {objects.labelroggenhour1,  objects.labelroggenhour2,  objects.labelroggenhour3};
    lv_obj_t* esche[3]        = {objects.labeleschehour1,   objects.labeleschehour2,   objects.labeleschehour3};
    for (int i = 0; i < 3; i++) {
      snprintf(hbuf, sizeof(hbuf), "%02d:00", pollen.h_slot[i]);
      setLabel(timeLabels[i], hbuf);
      setPollenLabel(birke[i],    openMeteoToDwd(pollen.h_birke[i]));
      setPollenLabel(graeser[i],  openMeteoToDwd(pollen.h_graeser[i]));
      setPollenLabel(erle[i],     openMeteoToDwd(pollen.h_erle[i]));
      setPollenLabel(hasel[i],    pollen.h_hasel[i]);
      setPollenLabel(beifuss[i],  openMeteoToDwd(pollen.h_beifuss[i]));
      setPollenLabel(ambrosia[i], openMeteoToDwd(pollen.h_ambrosia[i]));
      setPollenLabel(roggen[i],   pollen.h_roggen[i]);
      setPollenLabel(esche[i],    pollen.h_esche[i]);
    }
  }

  // ── Pollenwarnung (screenwarnungpollen) ─────────────────────
  // Basiert auf Open-Meteo naechste Stunde (umgerechnet auf DWD-Skala)
  float pollenWerte[] = {openMeteoToDwd(pollen.birke), openMeteoToDwd(pollen.erle),
                         openMeteoToDwd(pollen.graeser), openMeteoToDwd(pollen.beifuss),
                         openMeteoToDwd(pollen.ambrosia)};
  float maxPollen = -1.0f;
  for (int pi = 0; pi < 5; pi++) maxPollen = max(maxPollen, pollenWerte[pi]);
  pollenWarnAktiv = cfg.pollen_warn && (maxPollen > (float)cfg.pollen_schwelle);
  if (pollenWarnAktiv) {
    setLabel(objects.labelpollenwarntitel, "Pollenwarnung !");
    setLabel(objects.labelpollenwarnart,   schleimsterPollen().c_str());
    setLabel(objects.labelpollenwarnhint,  "Tippen zum Bestätigen");
  }

  // ── Luftqualität (screenairquality) ─────────────────────────
  {
    int aqi = pollen.aqi;
    // AQI-Arc: Skala 0–150, Werte >150 auf 150 kappen
    if (objects.arcaqi) {
      lv_arc_set_range(objects.arcaqi, 0, 150);
      lv_arc_set_value(objects.arcaqi, (aqi >= 0) ? min(aqi, 150) : 0);
      lv_obj_set_style_arc_color(objects.arcaqi, aqiColor(aqi), LV_PART_INDICATOR);
    }
    if (objects.labelaqivalue) {
      if (aqi >= 0) { snprintf(buf, sizeof(buf), "%d", aqi); }
      else          { snprintf(buf, sizeof(buf), "--"); }
      setLabelFmt(objects.labelaqivalue, aqiColor(aqi), buf);
    }
    if (objects.labelaqistatus)
      setLabelFmt(objects.labelaqistatus, aqiColor(aqi), aqiStatusText(aqi));

    // Einzelwerte – aktuelle Stunde mit Farbkodierung nach EU-Grenzwerten
    // Schwellen analog zum EU AQI: gut / mäßig / schlecht
    auto pm25Color = [](float v) -> lv_color_t {
      if (v < 0)   return lv_color_hex(0x888888);
      if (v <= 10) return lv_color_hex(0x50c878);
      if (v <= 25) return lv_color_hex(0xffd700);
      if (v <= 50) return lv_color_hex(0xff8c00);
      return lv_color_hex(0xff3030);
    };
    auto pm10Color = [](float v) -> lv_color_t {
      if (v < 0)    return lv_color_hex(0x888888);
      if (v <= 20)  return lv_color_hex(0x50c878);
      if (v <= 50)  return lv_color_hex(0xffd700);
      if (v <= 100) return lv_color_hex(0xff8c00);
      return lv_color_hex(0xff3030);
    };
    auto no2Color = [](float v) -> lv_color_t {
      if (v < 0)    return lv_color_hex(0x888888);
      if (v <= 40)  return lv_color_hex(0x50c878);
      if (v <= 100) return lv_color_hex(0xffd700);
      if (v <= 200) return lv_color_hex(0xff8c00);
      return lv_color_hex(0xff3030);
    };
    auto o3Color = [](float v) -> lv_color_t {
      if (v < 0)     return lv_color_hex(0x888888);
      if (v <= 60)   return lv_color_hex(0x50c878);
      if (v <= 120)  return lv_color_hex(0xffd700);
      if (v <= 180)  return lv_color_hex(0xff8c00);
      return lv_color_hex(0xff3030);
    };
    auto setAqiBar = [](lv_obj_t* bar, float val, int rangeMax) {
      if (!bar) return;
      lv_bar_set_range(bar, 0, rangeMax);
      lv_bar_set_value(bar, (val >= 0) ? (int)val : 0, LV_ANIM_OFF);
    };

    if (objects.labelpm25value) {
      snprintf(buf, sizeof(buf), (pollen.pm25 >= 0) ? "%.1f" : "--", pollen.pm25);
      setLabelFmt(objects.labelpm25value, pm25Color(pollen.pm25), buf);
    }
    setAqiBar(objects.bar_1, pollen.pm25, 75);

    if (objects.labelpm10value) {
      snprintf(buf, sizeof(buf), (pollen.pm10 >= 0) ? "%.1f" : "--", pollen.pm10);
      setLabelFmt(objects.labelpm10value, pm10Color(pollen.pm10), buf);
    }
    setAqiBar(objects.bar_2, pollen.pm10, 150);

    if (objects.labelno2value) {
      snprintf(buf, sizeof(buf), (pollen.no2 >= 0) ? "%.0f" : "--", pollen.no2);
      setLabelFmt(objects.labelno2value, no2Color(pollen.no2), buf);
    }
    setAqiBar(objects.bar_3, pollen.no2, 200);

    if (objects.labelo3value) {
      snprintf(buf, sizeof(buf), (pollen.o3 >= 0) ? "%.0f" : "--", pollen.o3);
      setLabelFmt(objects.labelo3value, o3Color(pollen.o3), buf);
    }
    setAqiBar(objects.bar_4, pollen.o3, 240);
  }

  // ── Biowetter (screenbiowetter + screenbiowetter2) ──────────
  // Periode 0 = heute Nmttg, 1 = morgen Vmttg → Screen 1
  // Periode 2 = morgen Nmttg, 3 = übermorgen Vmttg → Screen 2
  // Kategorien: 0=Befinden 1=BP-niedrig 2=BP-hoch 3=Rheuma-entz 4=Rheuma-degen 5=Asthma 6=Wärme
  {
    // Label-Arrays für beide Screens: [screen][periode_in_screen][kategorie]
    lv_obj_t* vmLabels[2][7] = {
      // Screen 1 – Periode 0 (Vmttg-Spalte = heute Nmttg)
      { objects.labelbiobevindenvm,   objects.labelbiobpniedrigvm,  objects.labelbiobphochvm,
        objects.labelbioreumentzvm,   objects.labelbioreumdegenvm,  objects.labelbioapsthmvm,
        objects.labelbiow_rmevm },
      // Screen 2 – Periode 2 (Vmttg-Spalte = morgen Vmttg)
      { objects.labelbiobevindenvm2,  objects.labelbiobpniedrigvm2, objects.labelbiobphochvm2,
        objects.labelbioreumentzvm2,  objects.labelbioreumdegenvm2, objects.labelbioapsthmvm2,
        objects.labelbiow_rmevm2 }
    };
    lv_obj_t* nmLabels[2][7] = {
      // Screen 1 – Periode 1 (Nmttg-Spalte = morgen Vmttg)
      { objects.labelbiobewindennm,   objects.labelbiobpniedrignm,  objects.labelbiobphochnm,
        objects.labelbioreumentznm,   objects.labelbioreumdegenm,   objects.labelbioapsthmam,
        objects.labelbiow_rmenm },
      // Screen 2 – Periode 3 (Nmttg-Spalte = übermorgen Vmttg)
      { objects.labelbiobewindennm2,  objects.labelbiobpniedrignm2, objects.labelbiobphochnm2,
        objects.labelbioreumentznm2,  objects.labelbioreumdegenm2,  objects.labelbioapsthmam2,
        objects.labelbiow_rmenm2 }
    };
    // Perioden-Indices im bio.wert[p][k] Array
    const int vmPeriod[2] = {0, 2};
    const int nmPeriod[2] = {1, 3};

    for (int s = 0; s < 2; s++) {
      for (int k = 0; k < 7; k++) {
        if (vmLabels[s][k]) {
          int v = bio.geladen ? bio.wert[vmPeriod[s]][k] : 0;
          setLabelFmt(vmLabels[s][k], bioWertColor(v), bioWertKurz(v));
        }
        if (nmLabels[s][k]) {
          int v = bio.geladen ? bio.wert[nmPeriod[s]][k] : 0;
          setLabelFmt(nmLabels[s][k], bioWertColor(v), bioWertKurz(v));
        }
      }
    }
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

// Hauptnavigation: screen_1 → forecastwetter → forecastpollen → screenwarnkarte1 → screensunmoon → screenairquality → screen_1
// Untermenüs: forecastpollen ↔ forecastpollenhour | screenwarnkarte1 ↔ screenwarnkarte2
static void cbHome(lv_event_t*)   { loadScreen(SCREEN_ID_SCREEN_1); }

// ── Menü-Screen Callbacks ────────────────────────────────────────────────────
static void cbMenuBrightness(lv_event_t* e) {
  lv_obj_t* arc = lv_event_get_target(e);
  int val = lv_arc_get_value(arc);
  cfg.brightness = val;
  setBrightness(val);
  Preferences prefs; prefs.begin("wcp", false);
  prefs.putInt("bright", val);
  prefs.end();
}

static void cbMenuRegenSwitch(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  cfg.regen_warn = lv_obj_has_state(sw, LV_STATE_CHECKED);
  Preferences prefs; prefs.begin("wcp", false);
  prefs.putBool("regen_warn", cfg.regen_warn);
  prefs.end();
}

static void cbMenuPollenSwitch(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  cfg.pollen_warn = lv_obj_has_state(sw, LV_STATE_CHECKED);
  Preferences prefs; prefs.begin("wcp", false);
  prefs.putBool("pollen_warn", cfg.pollen_warn);
  prefs.end();
}

static void cbMenuDwdSwitch(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  cfg.warn_region = lv_obj_has_state(sw, LV_STATE_CHECKED);
  Preferences prefs; prefs.begin("wcp", false);
  prefs.putBool("warn_reg", cfg.warn_region);
  prefs.end();
  // DWD-Warn-Button auf screen_1 sofort anpassen
  if (!cfg.warn_region && dwdWarnBtn) {
    lv_obj_add_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
    if (dwdBlinker) { lv_timer_del(dwdBlinker); dwdBlinker = nullptr; }
  }
}

static void cbMenu(lv_event_t*) {
  loadScreen(SCREEN_ID_SCREENMENU);
  // Switch-Zustände und Arc auf aktuelle cfg-Werte setzen
  if (objects.regenswitch) {
    if (cfg.regen_warn) lv_obj_add_state(objects.regenswitch, LV_STATE_CHECKED);
    else                lv_obj_clear_state(objects.regenswitch, LV_STATE_CHECKED);
  }
  if (objects.pollenswitch) {
    if (cfg.pollen_warn) lv_obj_add_state(objects.pollenswitch, LV_STATE_CHECKED);
    else                 lv_obj_clear_state(objects.pollenswitch, LV_STATE_CHECKED);
  }
  if (objects.dwdswitch) {
    if (cfg.warn_region) lv_obj_add_state(objects.dwdswitch, LV_STATE_CHECKED);
    else                 lv_obj_clear_state(objects.dwdswitch, LV_STATE_CHECKED);
  }
  if (objects.labellightchangevalue)
    lv_arc_set_value(objects.labellightchangevalue, cfg.brightness);
}
static void cbFwd1(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENFORECASTWETTER); }       // screen_1 >
static void cbBack1(lv_event_t*) { loadScreen(SCREEN_ID_SCREEN_1); }                   // forecastwetter <
static void cbFwd2(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENFORECASTPOLLEN); }       // forecastwetter >
static void cbBack2(lv_event_t*) { loadScreen(SCREEN_ID_SCREENFORECASTWETTER); }       // forecastpollen <
static void cbFwd3(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENWARNKARTE1); }           // forecastpollen >
static void cbBack3(lv_event_t*) { loadScreen(SCREEN_ID_SCREENFORECASTPOLLEN); }       // screenwarnkarte1 <
static void cbFwd4(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENSUNMOON); }              // screenwarnkarte1 >
static void cbFwd5(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENAIRQUALITY); }            // screensunmoon >
static void cbBack5(lv_event_t*) { loadScreen(SCREEN_ID_SCREENWARNKARTE1); }           // screensunmoon <
static void cbFwd6(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENBIOWETTER); }            // screenairquality > → biowetter
static void cbBack6(lv_event_t*) { loadScreen(SCREEN_ID_SCREENSUNMOON); }              // screenairquality <
static void cbBio(lv_event_t*)   { loadScreen(SCREEN_ID_SCREENBIOWETTER); }            // screen_1 < → biowetter
static void cbBack7(lv_event_t*) { loadScreen(SCREEN_ID_SCREENAIRQUALITY); }           // biowetter < → screenairquality
static void cbFwd7(lv_event_t*)  { loadScreen(SCREEN_ID_SCREEN_1); }                   // biowetter > → screen_1
static void cbHubBio(lv_event_t*)     { loadScreen(SCREEN_ID_SCREENBIOWETTER2); }      // biowetter → biowetter2
static void cbHubBioBack(lv_event_t*) { loadScreen(SCREEN_ID_SCREENBIOWETTER); }       // biowetter2 → biowetter
// Untermenü-Buttons (Hub)
static void cbHubPollen(lv_event_t*)     { loadScreen(SCREEN_ID_SCREENFORECASTPOLLENHOUR); } // forecastpollen → stündlich
static void cbHubPollenBack(lv_event_t*) { loadScreen(SCREEN_ID_SCREENFORECASTPOLLEN); }     // stündlich → tages
static void cbHubWarn(lv_event_t*) {
  loadScreen(SCREEN_ID_SCREENWARNKARTE2);
  aktualisiereWarnliste();
}
static void cbHubWarnBack(lv_event_t*)   { loadScreen(SCREEN_ID_SCREENWARNKARTE1); }         // warnkarte2 → warnkarte1

// ── DWD-Warn-Indikator auf screen_1 ─────────────────────────
static void cbDwdBlinker(lv_timer_t*) {
  if (!dwdWarnBtn || dwdWarnBestaetigt) return;
  if (lv_obj_has_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN))
    lv_obj_clear_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
}

static void cbDwdWarnBtnTap(lv_event_t*) {
  dwdWarnBestaetigt = true;
  if (dwdBlinker) { lv_timer_del(dwdBlinker); dwdBlinker = nullptr; }
  if (dwdWarnBtn) lv_obj_clear_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
  loadScreen(SCREEN_ID_SCREENWARNKARTE2);
  aktualisiereWarnliste();
}

void erstelleDwdWarnBtn() {
  if (dwdWarnBtn) return;
  // Mittig in der Nav-Reihe (y=273, h=40 – gleiche Zeile wie labelbuttonforward)
  dwdWarnBtn = lv_btn_create(objects.screen_1);
  lv_obj_set_size(dwdWarnBtn, 160, 40);
  lv_obj_set_pos(dwdWarnBtn, (480 - 160) / 2, 273);
  lv_obj_set_style_bg_color(dwdWarnBtn, lv_color_hex(0xcc0000), 0);
  lv_obj_set_style_bg_opa(dwdWarnBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(dwdWarnBtn, 6, 0);
  lv_obj_set_style_border_width(dwdWarnBtn, 0, 0);
  lv_obj_t* lbl = lv_label_create(dwdWarnBtn);
  lv_label_set_text(lbl, LV_SYMBOL_WARNING "  DWD Warnung");
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl);
  lv_obj_add_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(dwdWarnBtn, cbDwdWarnBtnTap, LV_EVENT_CLICKED, nullptr);
}

void aktualisiereDwdWarnBtn() {
  if (!dwdWarnBtn) return;
  if (anzahl_warnungen > 0) {
    lv_obj_clear_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
    if (!dwdWarnBestaetigt && !dwdBlinker)
      dwdBlinker = lv_timer_create(cbDwdBlinker, 600, nullptr);
  } else {
    lv_obj_add_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
    dwdWarnBestaetigt = false;
    if (dwdBlinker) { lv_timer_del(dwdBlinker); dwdBlinker = nullptr; }
  }
}

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

// ── Warn-Popup (Detail-Overlay auf dem Warnscreen) ──────────

static void cbWarnPopupOk(lv_event_t*) {
  if (warnPopup) {
    lv_obj_del(warnPopup);
    warnPopup = nullptr;
  }
  if (warnPopupNurSchliessen) return;
  if (warnPopupIstRegen) {
    regenWarnBestaetigt = true;
  } else {
    pollenWarnBestaetigt = true;
  }
  loadScreen(SCREEN_ID_SCREEN_1);
}

static void zeigeWarnPopup(lv_obj_t* parent, bool istRegen, const char* titel, const char* text, bool nurSchliessen) {
  if (warnPopup) { lv_obj_del(warnPopup); warnPopup = nullptr; }
  warnPopupIstRegen    = istRegen;
  warnPopupNurSchliessen = nurSchliessen;

  // Halbtransparentes dunkles Overlay über dem Warnscreen
  warnPopup = lv_obj_create(parent);
  lv_obj_set_size(warnPopup, 440, 270);
  lv_obj_align(warnPopup, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(warnPopup, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(warnPopup, LV_OPA_90, 0);
  lv_obj_set_style_border_color(warnPopup, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_width(warnPopup, 2, 0);
  lv_obj_set_style_radius(warnPopup, 10, 0);
  lv_obj_clear_flag(warnPopup, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* lblTitel = lv_label_create(warnPopup);
  lv_label_set_text(lblTitel, titel);
  lv_obj_set_style_text_font(lblTitel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(lblTitel, lv_color_hex(0xffcc00), 0);
  lv_obj_align(lblTitel, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t* lblText = lv_label_create(warnPopup);
  lv_label_set_long_mode(lblText, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(lblText, 400, 180);
  lv_label_set_text(lblText, text);
  lv_obj_set_style_text_font(lblText, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lblText, lv_color_hex(0xeeeeee), 0);
  lv_obj_align(lblText, LV_ALIGN_TOP_MID, 0, 38);

  lv_obj_t* btn = lv_btn_create(warnPopup);
  lv_obj_set_size(btn, 160, 40);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_t* lblBtn = lv_label_create(btn);
  lv_label_set_text(lblBtn, LV_SYMBOL_OK "  OK");
  lv_obj_set_style_text_color(lblBtn, lv_color_hex(0xffffff), 0);
  lv_obj_center(lblBtn);
  lv_obj_add_event_cb(btn, cbWarnPopupOk, LV_EVENT_CLICKED, nullptr);
}

static void cbWarnkarte2Tap(lv_event_t*) {
  // Karten-Tap wird über cbWarnKarteCardTap abgewickelt; Screen-Tap schließt offenes Popup
  if (warnPopup) { lv_obj_del(warnPopup); warnPopup = nullptr; }
}

static void cbWarnTap(lv_event_t* e) {
  lv_obj_t* src = lv_event_get_current_target(e);
  if (warnPopup) return; // Popup schon offen – nur OK-Button schließt

  if (src == objects.screenwarnung) {
    String details = "";
    for (int i = 0; i < anzahl_warnungen && i < 3; i++) {
      if (i > 0) details += "\n\n";
      details += ohneUmlaute(warnungen[i].titel);
      if (!warnungen[i].beschreibung.isEmpty())
        details += "\n" + ohneUmlaute(warnungen[i].beschreibung).substring(0, 100);
    }
    if (details.isEmpty()) details = "Regen in den naechsten 60 Min. erwartet.";
    zeigeWarnPopup(objects.screenwarnung, true, "Wetterwarnung", details.c_str(), false);

  } else if (src == objects.screenwarnungpollen) {
    String pollenName = ohneUmlaute(schleimsterPollen());
    float pollenWerte[] = {openMeteoToDwd(pollen.birke), openMeteoToDwd(pollen.erle),
                           openMeteoToDwd(pollen.graeser), openMeteoToDwd(pollen.beifuss),
                           openMeteoToDwd(pollen.ambrosia)};
    float maxP = -1; for (int i=0;i<5;i++) maxP=max(maxP,pollenWerte[i]);
    const char* stufe = maxP>=3?"stark":(maxP>=2?"hoch":(maxP>=1?"mittel":"gering"));
    String msg = pollenName + ": " + stufe + " Belastung\n(Stufe " + String((int)maxP) + " von 3)";
    msg += "\nSchwelle: >" + String(cfg.pollen_schwelle);
    zeigeWarnPopup(objects.screenwarnungpollen, false, "Pollenwarnung", msg.c_str(), false);
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
  erstelleWarnkarte2Screen();

  // Navigations-Buttons sofort verdrahten (vor WiFi-Check, gilt auch im Portal-Modus)
  // Hauptkette: screen_1 → forecastwetter → forecastpollen → screenwarnkarte1 → screensunmoon → screenairquality → screen_1
  // Seitenpfad: screen_1 ← (back) → screenbiowetter ↔ screenbiowetter2
  #define REG_CB(obj, cb, evt) do { if (obj) lv_obj_add_event_cb(obj, cb, evt, nullptr); } while(0)
  // Hauptkette Pfeile
  REG_CB(objects.labelbuttonforward,    cbFwd1,  LV_EVENT_CLICKED);  // screen_1 >
  REG_CB(objects.labelbuttonbackward_2, cbBack1, LV_EVENT_CLICKED);  // forecastwetter <
  REG_CB(objects.labelbuttonforward_2,  cbFwd2,  LV_EVENT_CLICKED);  // forecastwetter >
  REG_CB(objects.labelbuttonbackward_1, cbBack2, LV_EVENT_CLICKED);  // forecastpollen <
  REG_CB(objects.labelbuttonforward_1,  cbFwd3,  LV_EVENT_CLICKED);  // forecastpollen >
  REG_CB(objects.labelbuttonbackward_3, cbBack3, LV_EVENT_CLICKED);  // screenwarnkarte1 <
  REG_CB(objects.labelbuttonforward_3,  cbFwd4,  LV_EVENT_CLICKED);  // screenwarnkarte1 >
  REG_CB(objects.labelbuttonbackward_4, cbBack5, LV_EVENT_CLICKED);  // screensunmoon < (war button_1)
  REG_CB(objects.labelbuttonforward_4,  cbFwd5,  LV_EVENT_CLICKED);  // screensunmoon >
  REG_CB(objects.labelbuttonbackward_5, cbBack6, LV_EVENT_CLICKED);  // screenairquality <
  REG_CB(objects.labelbuttonforward_5,  cbFwd6,  LV_EVENT_CLICKED);  // screenairquality >
  // Seitenpfad Biowetter
  REG_CB(objects.labelbuttonbackward,      cbBio,       LV_EVENT_CLICKED);  // screen_1 < → biowetter
  REG_CB(objects.labelbuttonbackward_4_1,  cbBack7,     LV_EVENT_CLICKED);  // biowetter < → screen_1
  REG_CB(objects.labelbuttonforward_6,     cbFwd7,      LV_EVENT_CLICKED);  // biowetter > → screen_1
  REG_CB(objects.labelbuttonscreenhub_2,   cbHubBio,    LV_EVENT_CLICKED);  // biowetter M → biowetter2
  REG_CB(objects.labelbuttonscreenhubback_2, cbHubBioBack, LV_EVENT_CLICKED); // biowetter2 → biowetter
  // Untermenü Hub-Buttons
  REG_CB(objects.labelbuttonscreenhub,       cbHubPollen,    LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonscreenhubback,   cbHubPollenBack,LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonscreenhub_1,     cbHubWarn,      LV_EVENT_CLICKED);
  REG_CB(objects.labelbuttonscreenhubback_1, cbHubWarnBack,  LV_EVENT_CLICKED);
  // Menü-Icon auf screen_1 (fc_settings Bild) + Zurück-Button
  if (objects.fc_settings) lv_obj_add_flag(objects.fc_settings, LV_OBJ_FLAG_CLICKABLE);
  REG_CB(objects.fc_settings,   cbMenu, LV_EVENT_CLICKED);  // screen_1 → screenmenu
  REG_CB(objects.labelbuttonmenu_2, cbHome, LV_EVENT_CLICKED);  // screenmenu → screen_1
  // Menü-Screen Controls
  REG_CB(objects.labellightchangevalue, cbMenuBrightness,   LV_EVENT_VALUE_CHANGED);
  REG_CB(objects.regenswitch,           cbMenuRegenSwitch,  LV_EVENT_VALUE_CHANGED);
  REG_CB(objects.pollenswitch,          cbMenuPollenSwitch, LV_EVENT_VALUE_CHANGED);
  REG_CB(objects.dwdswitch,             cbMenuDwdSwitch,    LV_EVENT_VALUE_CHANGED);
  // Home-Buttons
  REG_CB(objects.labelbuttonhome,   cbHome, LV_EVENT_CLICKED);  // forecastpollen
  REG_CB(objects.labelbuttonhome_1, cbHome, LV_EVENT_CLICKED);  // forecastpollenhour
  REG_CB(objects.labelbuttonhome_2, cbHome, LV_EVENT_CLICKED);  // forecastwetter
  REG_CB(objects.labelbuttonhome_3, cbHome, LV_EVENT_CLICKED);  // screenwarnkarte1
  REG_CB(objects.labelbuttonhome_4, cbHome, LV_EVENT_CLICKED);  // screenwarnkarte2
  REG_CB(objects.labelbuttonhome_5, cbHome, LV_EVENT_CLICKED);  // screensunmoon
  REG_CB(objects.labelbuttonhome_6, cbHome, LV_EVENT_CLICKED);  // screenairquality
  REG_CB(objects.labelbuttonhome_7, cbHome, LV_EVENT_CLICKED);  // screenbiowetter
  REG_CB(objects.labelbuttonhome_8, cbHome, LV_EVENT_CLICKED);  // screenbiowetter2
  // Warn-Screens
  REG_CB(objects.screenwarnung,       cbWarnTap,       LV_EVENT_CLICKED);
  REG_CB(objects.screenwarnungpollen, cbWarnTap,       LV_EVENT_CLICKED);
  REG_CB(objects.screenwarnkarte2,    cbWarnkarte2Tap, LV_EVENT_CLICKED);
  #undef REG_CB
  // Warnliste-Screen ist clickbar für Popup (registriert via REG_CB auf screenwarnkarte2)

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

  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  zeigeBootScreen("Synchronisiere Zeit...");
  setzeBootFortschritt(45);
  { struct tm ti; int r = 0; while (!getLocalTime(&ti) && r++ < 30) { delay(300); lv_timer_handler(); } }

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
  setzeBootFortschritt(92);
  zeigeBootScreen("Lade Biowetter...");
  fetchBioWetter();
  setzeBootFortschritt(95);
  zeigeBootScreen("Lade Radarkarte...");
  fetchWmsRadar();
  letztesWarnkarteUpdate = millis();
  setzeBootFortschritt(100);

  // UI vollständig befüllen bevor der Hauptscreen erscheint
  aktualisiereUI();
  erstelleDwdWarnBtn();
  aktualisiereDwdWarnBtn();
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
    // Warnung nach jedem stündlichen Update zurücksetzen damit neue Belastung erneut warnt
    pollenWarnGezeigt    = false;
    pollenWarnBestaetigt = false;
  }

  // Biowetter alle 3 Stunden (DWD aktualisiert 2× täglich)
  if (letztesBioUpdate == 0 || millis() - letztesBioUpdate >= 10800000UL) {
    letztesBioUpdate = millis();
    fetchBioWetter();
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
    aktualisiereDwdWarnBtn();
    if (lv_scr_act() == objects.screenwarnkarte2) aktualisiereWarnliste();
  }

  // DWD-Warnkarte alle 30 Minuten
  if (millis() - letztesWarnkarteUpdate >= 600000UL) {
    letztesWarnkarteUpdate = millis();
    fetchWmsRadar();
  }

  // Nachtmodus (zeitbasiert)
  if (cfg.night_mode) {
    struct tm ti; getLocalTime(&ti);
    int nowMin = ti.tm_hour * 60 + ti.tm_min;
    bool inNight;
    if (cfg.night_from < cfg.night_to) {
      inNight = (nowMin >= cfg.night_from && nowMin < cfg.night_to);
    } else {
      inNight = (nowMin >= cfg.night_from || nowMin < cfg.night_to);  // über Mitternacht
    }
    if (inNight && !dimmed) {
      setBrightness(cfg.night_brightness);
      dimmed = true;
    } else if (!inNight && dimmed) {
      setBrightness(cfg.brightness);
      dimmed = false;
    }
  }

  // Display-Dimmen (Inaktivitäts-Timeout)
  if (!cfg.night_mode && cfg.dim_timeout > 0 && !dimmed) {
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

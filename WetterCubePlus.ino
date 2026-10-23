// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// ============================================================
//  WetterCubePlus.ino
//  ESP32-S3 N16R8 | ILI9488 3.5" 480x320 | XPT2046 Touch
//  LVGL 8.x | LovyanGFX | HTTP-OTA | WebUI | DWD-Warnungen
//  
// ============================================================

#include "webui_html.h"

// ---- Versions-Define (muss mit docs/version.json übereinstimmen!) ----
#define FIRMWARE_VERSION "2.1.10"
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
#include "src/ui/colors.h"
#include "src/ui/images.h"
#include "src/ui/fonts/fonts.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <WebServer.h>
#include <esp_random.h>
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include <stdarg.h>

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
  String web_pw;                 // Passwort der Web-Oberflaeche als "Salz:Hash" (leer = kein Passwort)
  String location;
  float  lat = 0.0f;
  float  lon = 0.0f;
  int    dwd_region = 31;       // 31 = Berlin; vollständige Liste siehe unten
  bool   pollen_warn = true;
  bool   hinweis_regen = true;   // Hinweis-Kachel auf Screen 1: Regen / Frost / UV
  bool   hinweis_frost = true;
  bool   hinweis_uv    = true;
  bool   hinweis_wind  = true;
  bool   verlauf_regen = true;   // Regenbalken im Temperaturverlauf
  int    main_box      = 0;   // rechtes Feld auf Screen 1: 0 = Pollen, 1 = Biowetter
  int    pollen_schwelle = 2;   // 0=gering 1=mittel 2=hoch 3=stark
  int    pollen_auswahl  = 0xFF; // gewaehlte Allergene, Bit i = POLLEN_ANZEIGE[i] (Standard: alle)
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
  int    ampel_modus      = 0;   // 0 = Temperatur, 1 = Pollen, 2 = Luftqualitaet
  String bio_zone = "A";        // DWD Biowetter-Zone A–J
  // Biowetter-Kategorien (Index 0–6): 0=Allg.Befinden, 1=Hypotonie, 2=Bluthochdruck, 3=Rheuma entz., 4=Rheuma deg., 5=Asthma, 6=Wärmebelast.
  int ws_cat0 = 0;
  int ws_cat1 = 1;
  int ws_cat2 = 2;
  int ws_cat3 = 5;
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
  float  uv_next3h   = 0.0f;  // Max UV der nächsten 3 Stunden
  float  uv_hinweis  = 0.0f;  // Max UV von jetzt bis in 3 Stunden (fuer den UV-Hinweis)
  float  boe_max     = 0.0f;  // staerkste Windboe der naechsten 6 Stunden in km/h (fuer den Sturm-Hinweis)
  bool   boe_jetzt   = false; // Boeen ueber der Schwelle schon in der aktuellen Stunde
  String boe_ab;              // Uhrzeit der ersten Stunde mit Boeen ueber der Schwelle
  float  frost_min   = 99.0f; // tiefste Temperatur der naechsten 12 Stunden (99 = keine Daten)
  bool   frost_jetzt = false; // schon jetzt <= 0 Grad
  String frost_ab;            // Uhrzeit der ersten Stunde mit <= 0 Grad
  // Temperaturverlauf: jetzt bis +24 Stunden (25 Punkte), stuendlich
  float  vl_temp[25]  = {};
  float  vl_regen[25] = {};   // Niederschlag je Stunde in mm
  int    vl_n         = 0;    // Anzahl gueltiger Punkte
  int    vl_h0        = 0;    // Stunde (0-23) des ersten Punkts
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
  // Regen-Nowcast (Open-Meteo minutely_15): Startzeit und Niederschlag je 15-Min-Slot
  static const int NC_MAX = 12;
  time_t nc_start[NC_MAX] = {};
  float  nc_mm[NC_MAX]    = {};
  int    nc_n             = 0;
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
  bool gueltig[4];      // Periode hat Werte vom DWD (nur lauter "kein Einfluss" ist gueltig und etwas anderes als "keine Daten")
  bool geladen = false;
  BioWetterDaten() { memset(wert, 0, sizeof(wert)); memset(gueltig, 0, sizeof(gueltig)); }
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
bool   dwdWarnAktiv           = false;
bool   dwdWarnBestaetigt      = false;

static lv_obj_t*  dwdWarnBtn    = nullptr;
static lv_timer_t* dwdBlinker   = nullptr;

// Update-Hinweis: automatische Versionspruefung + Badge auf Screen 1
#define UPDATE_CHECK_ERST      120000UL    // 2 Min. nach dem Start
#define UPDATE_CHECK_INTERVALL 21600000UL  // danach alle 6 Stunden
#define UPDATE_CHECK_RETRY     3600000UL   // nach einem Fehler nach 1 Stunde erneut
static lv_obj_t* updateBadge       = nullptr;
static lv_obj_t* updatePopup       = nullptr;
bool          updateVerfuegbar     = false;
String        updateNeueVersion    = "";
unsigned long letzterUpdateCheck   = 0;
unsigned long updateCheckIntervall = UPDATE_CHECK_ERST;
bool          otaStartAngefordert  = false;   // Installation per Touch: wird in loop() gestartet, nicht im LVGL-Callback

unsigned long letztesDatenUpdate  = 0;
unsigned long letzterWetterFetchErfolg = 0;  // millis() des letzten erfolgreichen fetchWetter() — für /api/ampel Staleness-Check
unsigned long letzteAmpelAnfrage = 0;        // millis() der letzten bedienten /api/ampel-Anfrage — zeigt ob der Server für die WetterAmpel erreichbar war

// ============================================================
//  Diagnose-Log (Ring-Puffer im RAM, per Browser unter /log abrufbar)
//  — nötig weil das Gerät nur per OTA/WLAN erreichbar ist, kein USB/Serial vor Ort.
// ============================================================
#define LOG_RING_SIZE 40
#define LOG_LINE_LEN  128
static char logRing[LOG_RING_SIZE][LOG_LINE_LEN];
static int  logRingPos   = 0;
static int  logRingCount = 0;

void logDiag(const char* fmt, ...) {
  char msg[LOG_LINE_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  char* slot = logRing[logRingPos];
  struct tm ti;
  if (getLocalTime(&ti, 0)) {
    snprintf(slot, LOG_LINE_LEN, "%02d:%02d:%02d %s", ti.tm_hour, ti.tm_min, ti.tm_sec, msg);
  } else {
    snprintf(slot, LOG_LINE_LEN, "[%lus] %s", millis() / 1000, msg);
  }
  logRingPos = (logRingPos + 1) % LOG_RING_SIZE;
  if (logRingCount < LOG_RING_SIZE) logRingCount++;

  Serial.println(slot);  // weiterhin auch auf Serial, falls doch mal USB dran ist
}

// Zeitpunkt des letzten erfolgreichen Abrufs je Datenquelle (fuer den Kopf von /log). Erfolge werden nicht einzeln
// protokolliert, damit der Ringpuffer fuer Fehler frei bleibt; Fehlschlaege stehen mit Grund im Log.
enum { AB_WETTER = 0, AB_DWD_POLLEN, AB_POLLEN_OM, AB_WARNUNGEN, AB_RADAR, AB_BIO, AB_ANZAHL };
static const char* const AB_NAME[AB_ANZAHL] = {"Wetter", "DWD-Pollen", "Pollen/Luft", "DWD-Warnungen", "Radar", "Biowetter"};
static unsigned long abrufErfolg[AB_ANZAHL] = {0};
static void abrufOk(int quelle) { abrufErfolg[quelle] = millis() ? millis() : 1; }

// Start-Diagnose: warum wurde neu gestartet, wie lief die WLAN-Verbindung (Anzeige im /log)
String          resetGrundText = "";

// Absturz-Spur: letzte Tätigkeit steht im RTC-Speicher und bleibt bei einem Panic/Watchdog-Neustart erhalten.
// Nach einem Absturz steht sie im /log ("Zuletzt aktiv vor dem Absturz").
static RTC_NOINIT_ATTR char     spurText[48];
static RTC_NOINIT_ATTR uint32_t spurMagic;
static String spurVorAbsturz = "";
static inline void spur(const char* was) {
  strlcpy(spurText, was, sizeof(spurText));
  spurMagic = 0x5350554Cu;
}
String          wifiStartInfo  = "";
volatile uint8_t wifiLetzterGrund = 0;   // letzter Disconnect-Grund des WLAN-Treibers

static const char* resetGrundName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "Strom angeschlossen";
    case ESP_RST_SW:        return "Software-Neustart (z. B. nach Update)";
    case ESP_RST_PANIC:     return "ABSTURZ (Panic)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "ABSTURZ (Watchdog)";
    case ESP_RST_BROWNOUT:  return "BROWNOUT (Spannungseinbruch, Netzteil/Kabel pruefen)";
    case ESP_RST_EXT:       return "Reset-Pin";
    case ESP_RST_DEEPSLEEP: return "Deep-Sleep";
    default:                return "unbekannt";
  }
}

static const char* wifiGrundName(uint8_t g) {
  switch (g) {
    case 0:   return "kein Fehlercode (Router antwortet nicht)";
    case 2:   return "Authentifizierung abgelaufen";
    case 15:  return "4-Wege-Handshake Timeout (Passwort?)";
    case 200: return "Beacon-Timeout";
    case 201: return "Netzwerk nicht gefunden (SSID/Reichweite/2,4 GHz?)";
    case 202: return "Authentifizierung fehlgeschlagen (Passwort?)";
    case 203: return "Assoziierung fehlgeschlagen";
    case 205: return "Verbindung fehlgeschlagen";
    default:  return "anderer Grund";
  }
}

static void wifiEventCb(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) wifiLetzterGrund = info.wifi_sta_disconnected.reason;
}

// LVGL-Speicher: Liegt der Pool im PSRAM (LV_MEM_CUSTOM, Schalter -DLV_MEM_PSRAM), liefert lv_mem_monitor()
// keine Werte; dann zaehlt der freie PSRAM.
static size_t lvglFreiBytes() {
#if LV_MEM_CUSTOM
  return ESP.getFreePsram();
#else
  lv_mem_monitor_t m; lv_mem_monitor(&m);
  return m.free_size;
#endif
}

static String lvglSpeicherText() {
#if LV_MEM_CUSTOM
  return "Pool im PSRAM (kein festes Limit), PSRAM frei " + String((unsigned)ESP.getFreePsram());
#else
  lv_mem_monitor_t m; lv_mem_monitor(&m);
  return "frei " + String((unsigned)m.free_size) + " von " + String((unsigned)m.total_size) + " Bytes (" +
         String((unsigned)m.used_pct) + " % belegt), groesster Block " + String((unsigned)m.free_biggest_size);
#endif
}
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
  cfg.web_pw          = prefs.getString("web_pw",     "");
  cfg.location        = prefs.getString("location",   "");
  cfg.lat             = prefs.getFloat ("lat",         0.0f);
  cfg.lon             = prefs.getFloat ("lon",         0.0f);
  cfg.dwd_region      = prefs.getInt   ("dwd_region",  31);
  cfg.pollen_warn     = prefs.getBool  ("pollen_warn", true);
  cfg.hinweis_regen   = prefs.getBool  ("hw_regen", true);
  cfg.hinweis_frost   = prefs.getBool  ("hw_frost", true);
  cfg.hinweis_uv      = prefs.getBool  ("hw_uv", true);
  cfg.hinweis_wind    = prefs.getBool  ("hw_wind", true);
  cfg.main_box        = prefs.getInt   ("main_box", 0);
  cfg.verlauf_regen   = prefs.getBool  ("vl_regen", true);
  cfg.pollen_schwelle = prefs.getInt   ("pol_schw",    2);
  cfg.pollen_auswahl  = prefs.getInt   ("pol_ausw",    0xFF);
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
  cfg.ampel_modus       = prefs.getInt   ("amp_modus",   0);
  cfg.bio_zone          = prefs.getString("bio_zone",    "A");
  cfg.ws_cat0           = prefs.getInt   ("ws_cat0",     0);
  cfg.ws_cat1           = prefs.getInt   ("ws_cat1",     1);
  cfg.ws_cat2           = prefs.getInt   ("ws_cat2",     2);
  cfg.ws_cat3           = prefs.getInt   ("ws_cat3",     5);
  prefs.end();
}

void speichereCfg() {
  prefs.begin("wcp", false);
  prefs.putString("ssid",       cfg.ssid);
  prefs.putString("pass",       cfg.pass);
  prefs.putString("web_pw",     cfg.web_pw);
  prefs.putString("location",   cfg.location);
  prefs.putFloat ("lat",        cfg.lat);
  prefs.putFloat ("lon",        cfg.lon);
  prefs.putInt   ("dwd_region", cfg.dwd_region);
  prefs.putBool  ("pollen_warn",cfg.pollen_warn);
  prefs.putBool  ("hw_regen",   cfg.hinweis_regen);
  prefs.putBool  ("hw_frost",   cfg.hinweis_frost);
  prefs.putBool  ("hw_uv",      cfg.hinweis_uv);
  prefs.putBool  ("hw_wind",    cfg.hinweis_wind);
  prefs.putInt   ("main_box",   cfg.main_box);
  prefs.putBool  ("vl_regen",   cfg.verlauf_regen);
  prefs.putInt   ("pol_schw",   cfg.pollen_schwelle);
  prefs.putInt   ("pol_ausw",   cfg.pollen_auswahl);
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
  prefs.putInt   ("amp_modus",   cfg.ampel_modus);
  prefs.putString("bio_zone",    cfg.bio_zone);
  prefs.putInt   ("ws_cat0",     cfg.ws_cat0);
  prefs.putInt   ("ws_cat1",     cfg.ws_cat1);
  prefs.putInt   ("ws_cat2",     cfg.ws_cat2);
  prefs.putInt   ("ws_cat3",     cfg.ws_cat3);
  prefs.end();
}

// ── HTTPS mit Zertifikatspruefung ────────────────────────────────────────────
// Das Server-Zertifikat wird gegen das im ESP32-Core eingebaute CA-Paket geprueft. Das klappt nur mit
// gueltiger Uhrzeit (Zertifikate haben ein Ablaufdatum); ohne NTP-Zeit schlaegt die Verbindung dann fehl.
static bool zeitGueltig() { return time(nullptr) > 1767225600; }   // nach dem 1. Januar 2026
static void tlsSicher(WiFiClientSecure& c) { c.useBuiltinCACertBundle(); }

// ============================================================
//  Geocoding
// ============================================================
bool geocodeLocation(const String& city) {
  WiFiClientSecure sc; tlsSicher(sc);
  HTTPClient http;
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=";
  url += city + "&count=1&language=de&format=json";
  logDiag("[Geo] Suche '%s'", city.c_str());
  http.begin(sc, url);
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      if (doc["results"].size() > 0) {
        cfg.lat = doc["results"][0]["latitude"].as<float>();
        cfg.lon = doc["results"][0]["longitude"].as<float>();
        logDiag("[Geo] Gefunden: %.4f, %.4f", cfg.lat, cfg.lon);
        http.end(); return true;
      }
      logDiag("[Geo] Fehlgeschlagen fuer '%s': kein Treffer", city.c_str());
    } else {
      logDiag("[Geo] Fehlgeschlagen fuer '%s': JSON-Fehler", city.c_str());
    }
  } else {
    logDiag("[Geo] Fehlgeschlagen fuer '%s': HTTP %d", city.c_str(), code);
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

void handleApiLog();

// Passwort des Einrichtungs-WLANs: 8 Ziffern (WPA2 verlangt mindestens 8 Zeichen), bei jedem Einrichtungsmodus neu, steht am Display
char portalPasswort[12] = "";
void portalPasswortNeu() { snprintf(portalPasswort, sizeof(portalPasswort), "%08u", (unsigned)(esp_random() % 100000000UL)); }

// ── Zugriffsschutz (Normalbetrieb) ───────────────────────────────────────────
// 1) Host-Pruefung (gegen DNS-Rebinding): Anfragen muessen an die IP-Adresse oder den Namen des Geraets gehen
// 2) Herkunftspruefung (gegen fremde Webseiten): schreibende Anfragen aus dem Browser nur von dieser Seite selbst
static bool hostErlaubt(String h) {
  h.toLowerCase();
  if (h.isEmpty()) return true;
  if (h[0] == '[') return true;                               // IPv6-Adresse
  int dp = h.indexOf(':'); if (dp >= 0) h = h.substring(0, dp);
  bool nurZiffern = true;
  for (size_t i = 0; i < h.length(); i++) if (!isdigit((unsigned char)h[i]) && h[i] != '.') { nurZiffern = false; break; }
  if (nurZiffern) return true;                                // IP-Adresse
  return h.startsWith(MDNS_NAME) || h.endsWith(".local") || h == "localhost";
}

// ── Passwort fuer die Web-Oberflaeche und "Passwort vergessen" ───────────────
static String sha256Hex(const String& s) {
  unsigned char d[32];
  mbedtls_sha256_context c;
  mbedtls_sha256_init(&c); mbedtls_sha256_starts(&c, 0);
  mbedtls_sha256_update(&c, (const unsigned char*)s.c_str(), s.length());
  mbedtls_sha256_finish(&c, d); mbedtls_sha256_free(&c);
  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + 2 * i, 3, "%02x", d[i]);
  return String(hex);
}

static void webPwSetzen(const String& pw) {
  char salz[17]; snprintf(salz, sizeof(salz), "%08x%08x", (unsigned)esp_random(), (unsigned)esp_random());
  cfg.web_pw = String(salz) + ":" + sha256Hex(String(salz) + pw);
}

static bool gleichLang(const String& a, const String& b) {
  unsigned char diff = (unsigned char)(a.length() ^ b.length());
  for (size_t i = 0; i < a.length() && i < b.length(); i++) diff |= (unsigned char)(a[i] ^ b[i]);
  return diff == 0;
}

static bool webPwPruefen(const String& pw) {
  int d = cfg.web_pw.indexOf(':');
  if (d != 16) return false;
  return gleichLang(cfg.web_pw.substring(d + 1), sha256Hex(cfg.web_pw.substring(0, d) + pw));
}

static bool anmeldungOk() {
  String a = server.header("Authorization");
  if (!a.startsWith("Basic ")) return false;
  String b64 = a.substring(6); b64.trim();
  unsigned char out[160]; size_t n = 0;
  if (mbedtls_base64_decode(out, sizeof(out) - 1, &n, (const unsigned char*)b64.c_str(), b64.length()) != 0) return false;
  out[n] = 0;
  String up = String((const char*)out);
  int c = up.indexOf(':');
  if (c < 0 || up.substring(0, c) != "admin") return false;
  return webPwPruefen(up.substring(c + 1));
}

static const char PW_SEITE_KOPF[] PROGMEM =
  "<!DOCTYPE html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>WetterCubePlus</title>"
  "<style>body{background:#0d1117;color:#e6edf3;font-family:Arial,sans-serif;max-width:480px;margin:auto;padding:20px}h2{color:#58a6ff}"
  ".k{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:12px 14px;margin:12px 0}"
  "input[type=text]{width:100%;box-sizing:border-box;padding:10px;font-size:18px;border-radius:8px;border:1px solid #30363d;background:#0d1117;color:#e6edf3}"
  "button{background:#2ea043;color:#fff;border:0;border-radius:8px;padding:11px 18px;font-size:15px;margin-top:12px;width:100%}a{color:#58a6ff}small{color:#8b949e}</style></head><body>";

static void antwort401() {
  server.sendHeader("WWW-Authenticate", "Basic realm=\"WetterCubePlus\", charset=\"UTF-8\"");
  server.send(401, "text/html; charset=utf-8", String(FPSTR(PW_SEITE_KOPF)) + "<h2>Anmeldung erforderlich</h2><div class='k'>Benutzername: <b>admin</b><br><a href='/passwort_vergessen'>Passwort vergessen?</a></div></body></html>");
}

// Code-Fenster am Display (nur ASCII: die eingebauten Schriften haben keine Umlaute)
static lv_obj_t* resetFenster = nullptr;
static lv_timer_t* resetTimer = nullptr;
static uint32_t resetFensterBis = 0;
static void resetFensterWeg() {
  if (resetTimer) { lv_timer_del(resetTimer); resetTimer = nullptr; }
  if (resetFenster) { lv_obj_del(resetFenster); resetFenster = nullptr; }
}
static void resetFensterZeigen(const char* code, unsigned sekunden) {
  resetFensterWeg();
  resetFenster = lv_obj_create(lv_layer_top());
  lv_obj_set_size(resetFenster, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_pos(resetFenster, 0, 0);
  lv_obj_set_style_bg_color(resetFenster, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(resetFenster, LV_OPA_90, 0);
  lv_obj_set_style_border_width(resetFenster, 0, 0);
  lv_obj_set_style_radius(resetFenster, 0, 0);
  lv_obj_clear_flag(resetFenster, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* t = lv_label_create(resetFenster);
  lv_label_set_text(t, "Passwort vergessen");
  lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(0xf5a623), 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_t* c = lv_label_create(resetFenster);
  lv_label_set_text(c, code);
  lv_obj_set_style_text_font(c, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(c, lv_color_hex(0xffffff), 0);
  lv_obj_align(c, LV_ALIGN_CENTER, 0, -6);
  lv_obj_t* h = lv_label_create(resetFenster);
  lv_label_set_text(h, "Gib diesen Code im Browser ein.\nEr gilt zwei Minuten.");
  lv_obj_set_style_text_font(h, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(0xc8cee8), 0);
  lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -36);
  resetFensterBis = lv_tick_get() + sekunden * 1000UL;
  resetTimer = lv_timer_create([](lv_timer_t*) { if ((int32_t)(lv_tick_get() - resetFensterBis) >= 0) resetFensterWeg(); }, 1000, nullptr);
  lv_timer_handler();
}

static void handlePwSpeichern() {
  if (server.hasArg("pw_weg")) {
    cfg.web_pw = ""; speichereCfg();
    logDiag("[Sicherheit] Web-Passwort entfernt");
  } else {
    String p1 = server.arg("pw1"), p2 = server.arg("pw2");
    if (p1.length() < 6 || p1.length() > 64) { server.send(400, "text/plain; charset=utf-8", "Das Passwort muss 6 bis 64 Zeichen lang sein."); return; }
    if (p1 != p2) { server.send(400, "text/plain; charset=utf-8", "Die beiden Passwoerter sind nicht gleich."); return; }
    webPwSetzen(p1); speichereCfg();
    logDiag("[Sicherheit] Web-Passwort gesetzt");
  }
  server.sendHeader("Location", "/#system"); server.send(303, "text/plain", "");
}

// "Passwort vergessen": Ein 6-stelliger Code erscheint am Display. Wer ihn im Browser eingibt, loescht das Passwort.
// Gueltig 2 Minuten, nach 5 falschen Eingaben 10 Minuten gesperrt, der Code laesst sich hoechstens alle 20 Sekunden neu anfordern.
static String s_resetCode;
static unsigned long s_resetBis = 0, s_resetSperreBis = 0, s_resetStart = 0;
static uint8_t s_resetFehl = 0;
static const unsigned long RESET_GUELTIG_MS = 120000UL, RESET_SPERRE_MS = 600000UL;

static String vergessenSeite(const String& meldung) {
  String h = String(FPSTR(PW_SEITE_KOPF)) + "<h2>Passwort vergessen</h2>";
  if (!meldung.isEmpty()) h += "<div class='k'><b>" + meldung + "</b></div>";
  if (cfg.web_pw.isEmpty()) return h + "<div class='k'>Es ist kein Passwort gesetzt. <a href='/'>Zu den Einstellungen</a></div></body></html>";
  h += "<div class='k'><small>1. Tippe auf den Knopf: Am Geraet erscheint ein Code (zwei Minuten gueltig).</small>"
       "<form method='POST' action='/passwort_vergessen/start'><button type='submit'>Code am Geraet anzeigen</button></form></div>"
       "<div class='k'><small>2. Gib den Code hier ein. Das Passwort wird dann geloescht.</small>"
       "<form method='POST' action='/passwort_vergessen/loeschen'><input type='text' name='code' inputmode='numeric' maxlength='6' autocomplete='off' placeholder='6-stelliger Code'>"
       "<button type='submit'>Passwort loeschen</button></form></div></body></html>";
  return h;
}

static bool resetGesperrt() { return s_resetSperreBis && (long)(millis() - s_resetSperreBis) < 0; }

static void handlePwVergessen() { server.send(200, "text/html; charset=utf-8", vergessenSeite("")); }

static void handlePwVergessenStart() {
  unsigned long jetzt = millis();
  if (cfg.web_pw.isEmpty()) { server.send(200, "text/html; charset=utf-8", vergessenSeite("")); return; }
  if (resetGesperrt()) { server.send(200, "text/html; charset=utf-8", vergessenSeite("Zu viele falsche Eingaben. Bitte zehn Minuten warten.")); return; }
  if (s_resetStart && jetzt - s_resetStart < 20000UL) { server.send(200, "text/html; charset=utf-8", vergessenSeite("Bitte kurz warten und dann noch einmal tippen.")); return; }
  char c[8]; snprintf(c, sizeof(c), "%06u", (unsigned)(esp_random() % 1000000UL));
  s_resetCode = c; s_resetBis = jetzt + RESET_GUELTIG_MS; s_resetStart = jetzt; s_resetFehl = 0;
  resetFensterZeigen(c, RESET_GUELTIG_MS / 1000);
  logDiag("[Sicherheit] Code zum Zuruecksetzen des Web-Passworts am Display gezeigt");
  server.send(200, "text/html; charset=utf-8", vergessenSeite("Der Code steht jetzt am Display des Geraets (zwei Minuten gueltig)."));
}

static void handlePwVergessenLoeschen() {
  unsigned long jetzt = millis();
  if (cfg.web_pw.isEmpty()) { server.send(200, "text/html; charset=utf-8", vergessenSeite("")); return; }
  if (resetGesperrt()) { server.send(200, "text/html; charset=utf-8", vergessenSeite("Zu viele falsche Eingaben. Bitte zehn Minuten warten.")); return; }
  String c = server.arg("code"); c.trim();
  const bool gueltig = s_resetCode.length() == 6 && (long)(s_resetBis - jetzt) > 0;
  if (gueltig && gleichLang(c, s_resetCode)) {
    cfg.web_pw = ""; speichereCfg();
    s_resetCode = ""; s_resetBis = 0; s_resetFehl = 0;
    resetFensterWeg();
    logDiag("[Sicherheit] Web-Passwort mit dem Code vom Display geloescht");
    server.sendHeader("Location", "/"); server.send(303, "text/plain", "");
    return;
  }
  if (++s_resetFehl >= 5) {
    s_resetSperreBis = jetzt + RESET_SPERRE_MS; s_resetFehl = 0; s_resetCode = ""; s_resetBis = 0;
    resetFensterWeg();
    logDiag("[Sicherheit] Zu viele falsche Codes, Zuruecksetzen 10 Minuten gesperrt");
    server.send(200, "text/html; charset=utf-8", vergessenSeite("Zu viele falsche Eingaben. Bitte zehn Minuten warten."));
    return;
  }
  server.send(200, "text/html; charset=utf-8", vergessenSeite(gueltig ? "Der Code stimmt nicht." : "Es gibt keinen gueltigen Code. Tippe erst auf Code am Geraet anzeigen."));
}

// Wird vor jedem Handler aufgerufen; false = die Anfrage wurde schon abgelehnt
static bool zugriffOk() {
  if (portal_modus) return true;                              // Einrichtungs-WLAN: eigenes Passwort, und das Portal braucht beliebige Namen
  String host = server.hostHeader();
  if (!hostErlaubt(host)) { server.send(403, "text/plain", "Zugriff abgelehnt: Das ist nicht die Adresse des Geraets."); return false; }
  if (server.method() != HTTP_GET && server.method() != HTTP_HEAD) {   // schreibende Anfragen: Browser melden ihre Herkunft
    String o = server.header("Origin");
    if (o.length() && !o.equalsIgnoreCase("http://" + host)) {
      logDiag("[Sicherheit] Anfrage von fremder Herkunft abgelehnt: %s", o.c_str());
      server.send(403, "text/plain", "Zugriff abgelehnt: Anfrage von einer fremden Seite.");
      return false;
    }
  }
  if (cfg.web_pw.isEmpty()) return true;
  String u = server.uri();
  if (u == "/api/ampel" || u.startsWith("/passwort_vergessen")) return true;   // Lese-Schnittstelle fuer die WetterAmpel; Weg zum Zuruecksetzen
  static unsigned long sperreBis = 0; static uint8_t fehl = 0;
  if (sperreBis && (long)(millis() - sperreBis) < 0) { server.send(429, "text/plain", "Zu viele falsche Anmeldungen. Bitte eine Minute warten."); return false; }
  if (anmeldungOk()) { fehl = 0; return true; }
  if (server.header("Authorization").length()) {                 // Anmeldedaten waren falsch (nicht nur die erste Anfrage ohne Daten)
    delay(500);
    if (++fehl >= 8) { fehl = 0; sperreBis = millis() + 60000UL; logDiag("[Sicherheit] Zu viele falsche Anmeldungen, 60 s gesperrt"); }
  }
  antwort401();
  return false;
}
#define GESCHUETZT(h) ([]() { if (!zugriffOk()) return; h(); })

void startePortal() {
  portal_modus = true;
  WiFi.disconnect(true); delay(100);
  WiFi.setAutoReconnect(false);   // kein Hintergrund-Scan, der das Portal-WLAN stoert
  WiFi.mode(WIFI_AP);
  if (!portalPasswort[0]) portalPasswortNeu();
  WiFi.softAP("WetterCubePlus-Setup", portalPasswort);   // WPA2, Passwort steht am Display
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/",          HTTP_GET,  handlePortalRoot);
  server.on("/speichern", HTTP_POST, handlePortalSave);
  server.on("/log",       HTTP_GET,  handleApiLog);   // Diagnose auch im Einrichtungsmodus: http://192.168.4.1/log
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302); });
  server.begin();

  // Boot-Screen mit Portal-Hinweis über LVGL (kein raw TFT, damit Buttons aktiv bleiben)
  zeigeBootScreen(String("WLAN: WetterCubePlus-Setup\nPasswort: ") + portalPasswort);
  setzeBootFortschritt(0);
  Serial.println("[Portal] AP gestartet: WetterCubePlus-Setup");
  logDiag("[Portal] Einrichtungsmodus: WLAN 'WetterCubePlus-Setup' (WPA2), http://192.168.4.1/");
}

// Einrichtungsmodus nach fehlgeschlagener Verbindung: alle 60 s erneut mit den gespeicherten Daten
// versuchen (z. B. Router war beim Start noch nicht bereit) und bei Erfolg neu starten.
// Nicht stoeren, solange jemand per Handy am Portal haengt.
static unsigned long letzterPortalRetry = 0;
static void portalWlanRetry() {
  if (cfg.ssid.isEmpty()) return;                       // nie eingerichtet: es gibt nichts zu verbinden
  if (WiFi.softAPgetStationNum() > 0) return;           // jemand konfiguriert gerade
  if (millis() - letzterPortalRetry < 60000UL) return;
  letzterPortalRetry = millis();

  WiFi.mode(WIFI_AP_STA);
  wifiLetzterGrund = 0;
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) {
    dnsServer.processNextRequest();
    server.handleClient();
    lv_timer_handler();
    delay(20);
  }
  if (WiFi.status() == WL_CONNECTED) {
    logDiag("[WLAN] Verbindung im Einrichtungsmodus hergestellt, Neustart");
    delay(500);
    ESP.restart();
  }
  logDiag("[WLAN] Wiederholung fehlgeschlagen: %s", wifiGrundName(wifiLetzterGrund));
  WiFi.disconnect(false, false);   // STA ruhigstellen, Portal-AP bleibt
}

// ============================================================
//  Web-Konfiguration (Normalbetrieb) – wettercubeplus.local
// ============================================================

// Vorwärts-Deklarationen
void handleWebRoot();
void handleWebSave();
static void ampelBerechnen(const char*& active, const char*& modus, String& detail);
void handleApiAmpel();
void handleApiAmpelSave();
void handleApiLog();
void handleWebOtaCheck();
void handleWebOtaDoUpdate();
void handleWebWlanAendern();
void handleWebWlanSave();

static const char* WS_CAT_NAMEN[] = {
  "Allg. Befinden", "Hypotonie", "Bluthochdruck", "Rheuma entz.", "Rheuma deg.", "Asthma", "Waermebelast."
};

static String baueWsCatOptions(int selected) {
  String out;
  for (int i = 0; i < 7; i++) {
    out += "<option value='" + String(i) + "'" + (i == selected ? " selected" : "") + ">" + WS_CAT_NAMEN[i] + "</option>";
  }
  return out;
}

static String baueBioZoneOptions(const String& sel) {
  static const struct { const char* id; const char* label; } zones[] = {
    {"A", "A – Schleswig-Holstein, Hamburg, nördl. Niedersachsen, Bremen"},
    {"B", "B – Mecklenburg-Vorpommern"},
    {"C", "C – SW-Niedersachsen, Nordrhein-Westfalen"},
    {"D", "D – Östl. und südl. Niedersachsen"},
    {"E", "E – Berlin, Brandenburg, nördl. Sachsen-Anhalt"},
    {"F", "F – Hessen, Rheinland-Pfalz, Saarland"},
    {"G", "G – Südl. Sachsen-Anhalt, Thüringen, Sachsen"},
    {"H", "H – Baden"},
    {"I", "I – Württemberg, Franken (ohne östl. Oberfranken)"},
    {"J", "J – Niederbayern, Oberpfalz, östl. Oberfranken"},
    {"K", "K – Schwaben, Oberbayern"},
  };
  String out;
  for (int i = 0; i < 11; i++) {
    out += "<option value='" + String(zones[i].id) + "'" + (sel == zones[i].id ? " selected" : "") + ">" + zones[i].label + "</option>";
  }
  return out;
}

// Schalter fuer "Meine Allergene" (Bit i = Allergen i, Reihenfolge wie POLLEN_ANZEIGE)
static String bauePollenAuswahl() {
  static const char* NAMEN_HTML[8] = {"Hasel", "Erle", "Esche", "Birke", "Gr&auml;ser", "Roggen", "Beifu&szlig;", "Ambrosia"};
  String out = "<input type=\"hidden\" name=\"pol_a_form\" value=\"1\">";
  for (int i = 0; i < 8; i++) {
    out += "<div class=\"row\"><span>" + String(NAMEN_HTML[i]) + "</span><label class=\"toggle\"><input type=\"checkbox\" name=\"pol_a" +
           String(i) + "\"" + (((cfg.pollen_auswahl >> i) & 1) ? " checked" : "") + "><span class=\"slider\"></span></label></div>";
  }
  return out;
}

void handleWebRoot() {
  String html = FPSTR(WEBUI_HTML);
  html.replace("%VERSION%",     FIRMWARE_VERSION);
  html.replace("%IP%",          WiFi.localIP().toString());
  html.replace("%WPSTATUS%",    cfg.web_pw.isEmpty() ? "Es ist kein Passwort gesetzt: Jeder im Heimnetz kann die Einstellungen aendern." : "Ein Passwort ist gesetzt (Benutzername: <b>admin</b>).");
  html.replace("%LOCATION%",    cfg.location);
  html.replace("%DWD_REGIONEN%",baueDwdRegionenOptions(cfg.dwd_region));
  html.replace("%PW%",          cfg.pollen_warn   ? "checked" : "");
  html.replace("%HR%",          cfg.hinweis_regen ? "checked" : "");
  html.replace("%HF%",          cfg.hinweis_frost ? "checked" : "");
  html.replace("%HU%",          cfg.hinweis_uv    ? "checked" : "");
  html.replace("%HW%",          cfg.hinweis_wind  ? "checked" : "");
  html.replace("%VR%",          cfg.verlauf_regen ? "checked" : "");
  html.replace("%MB0%",         cfg.main_box == 0 ? "selected" : "");
  html.replace("%MB1%",         cfg.main_box == 1 ? "selected" : "");
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
  for (int i = 0; i < 3; i++) html.replace("%AMP_M" + String(i) + "%", cfg.ampel_modus == i ? "selected" : "");
  html.replace("%POL_AUSWAHL%",       bauePollenAuswahl());
  html.replace("%BIO_ZONE_OPTIONS%",  baueBioZoneOptions(cfg.bio_zone));
  html.replace("%WS_CAT_OPTIONS0%",   baueWsCatOptions(cfg.ws_cat0));
  html.replace("%WS_CAT_OPTIONS1%",   baueWsCatOptions(cfg.ws_cat1));
  html.replace("%WS_CAT_OPTIONS2%",   baueWsCatOptions(cfg.ws_cat2));
  html.replace("%WS_CAT_OPTIONS3%",   baueWsCatOptions(cfg.ws_cat3));
  server.send(200, "text/html", html);
}

void handleWebSave() {
  String prevLocation = cfg.location;
  if (server.hasArg("location"))   cfg.location        = server.arg("location");
  if (server.hasArg("dwd_region")) cfg.dwd_region      = server.arg("dwd_region").toInt();
  cfg.pollen_warn     = server.hasArg("pollen_warn");
  cfg.hinweis_regen   = server.hasArg("hw_regen");
  cfg.hinweis_frost   = server.hasArg("hw_frost");
  cfg.hinweis_uv      = server.hasArg("hw_uv");
  cfg.hinweis_wind    = server.hasArg("hw_wind");
  if (server.hasArg("main_box"))   cfg.main_box        = constrain(server.arg("main_box").toInt(), 0, 1);
  cfg.verlauf_regen   = server.hasArg("vl_regen");
  { bool warnNeu = server.hasArg("warn_region");
    if (warnNeu != cfg.warn_region) { if (!warnNeu) anzahl_warnungen = 0; letztesWarnUpdate = millis() - 900000UL; }   // Hauptschleife frischt Warnungen und Button gleich auf
    cfg.warn_region = warnNeu; }
  cfg.night_mode      = server.hasArg("night_mode");
  if (server.hasArg("night_from"))   cfg.night_from       = server.arg("night_from").toInt();
  if (server.hasArg("night_to"))     cfg.night_to         = server.arg("night_to").toInt();
  if (server.hasArg("night_bright")) cfg.night_brightness = server.arg("night_bright").toInt();
  if (server.hasArg("pol_schw"))   cfg.pollen_schwelle = server.arg("pol_schw").toInt();
  if (server.hasArg("pol_a_form")) {   // Marker: nur auswerten, wenn die Allergen-Schalter wirklich mitgesendet wurden
    int m = 0;
    for (int i = 0; i < 8; i++) if (server.hasArg("pol_a" + String(i))) m |= (1 << i);
    cfg.pollen_auswahl = m;
  }
  if (server.hasArg("brightness")) cfg.brightness      = server.arg("brightness").toInt();
  if (server.hasArg("dim_time"))   cfg.dim_timeout     = server.arg("dim_time").toInt();
  if (server.hasArg("bio_zone"))   cfg.bio_zone        = server.arg("bio_zone");
  if (server.hasArg("ws_cat0"))    cfg.ws_cat0         = server.arg("ws_cat0").toInt();
  if (server.hasArg("ws_cat1"))    cfg.ws_cat1         = server.arg("ws_cat1").toInt();
  if (server.hasArg("ws_cat2"))    cfg.ws_cat2         = server.arg("ws_cat2").toInt();
  if (server.hasArg("ws_cat3"))    cfg.ws_cat3         = server.arg("ws_cat3").toInt();
  // Geocoding nur neu anstoßen wenn Location tatsächlich geändert
  if (cfg.location != prevLocation) { cfg.lat = 0.0f; cfg.lon = 0.0f; logDiag("[Config] Standort geaendert: '%s' -> '%s'", prevLocation.c_str(), cfg.location.c_str()); }
  speichereCfg();
  logDiag("[Config] gespeichert");
  setBrightness(cfg.brightness);
  aktualisiereUI();
  server.sendHeader("Location", "/"); server.send(302);
}

// ── Einstellungen sichern und wiederherstellen (ohne WLAN-Zugangsdaten) ──────────
void handleWebBackup() {
  DynamicJsonDocument doc(2048);
  doc["typ"]              = "WetterCubePlus-Einstellungen";
  doc["firmware"]         = FIRMWARE_VERSION;
  doc["location"]         = cfg.location;
  doc["lat"]              = cfg.lat;
  doc["lon"]              = cfg.lon;
  doc["dwd_region"]       = cfg.dwd_region;
  doc["pollen_warn"]      = cfg.pollen_warn;
  doc["hinweis_regen"]    = cfg.hinweis_regen;
  doc["hinweis_frost"]    = cfg.hinweis_frost;
  doc["hinweis_uv"]       = cfg.hinweis_uv;
  doc["hinweis_wind"]     = cfg.hinweis_wind;
  doc["main_box"]         = cfg.main_box;
  doc["verlauf_regen"]    = cfg.verlauf_regen;
  doc["pollen_schwelle"]  = cfg.pollen_schwelle;
  doc["pollen_auswahl"]   = cfg.pollen_auswahl;
  doc["warn_region"]      = cfg.warn_region;
  doc["dim_timeout"]      = cfg.dim_timeout;
  doc["brightness"]       = cfg.brightness;
  doc["night_mode"]       = cfg.night_mode;
  doc["night_from"]       = cfg.night_from;
  doc["night_to"]         = cfg.night_to;
  doc["night_brightness"] = cfg.night_brightness;
  doc["ampel_gruen_min"]  = cfg.ampel_gruen_min;
  doc["ampel_gruen_max"]  = cfg.ampel_gruen_max;
  doc["ampel_gelb_min"]   = cfg.ampel_gelb_min;
  doc["ampel_gelb_max"]   = cfg.ampel_gelb_max;
  doc["ampel_rot_min"]    = cfg.ampel_rot_min;
  doc["ampel_rot_max"]    = cfg.ampel_rot_max;
  doc["ampel_modus"]      = cfg.ampel_modus;
  doc["bio_zone"]         = cfg.bio_zone;
  doc["ws_cat0"]          = cfg.ws_cat0;
  doc["ws_cat1"]          = cfg.ws_cat1;
  doc["ws_cat2"]          = cfg.ws_cat2;
  doc["ws_cat3"]          = cfg.ws_cat3;
  String out;
  serializeJsonPretty(doc, out);
  server.sendHeader("Content-Disposition", "attachment; filename=wettercubeplus-einstellungen.json");
  server.send(200, "application/json", out);
}

static int rsKlemme(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void handleWebRestore() {
  String body = server.arg("plain");
  DynamicJsonDocument doc(2048);
  if (body.length() == 0 || deserializeJson(doc, body) != DeserializationError::Ok ||
      String((const char*)(doc["typ"] | "")) != "WetterCubePlus-Einstellungen") {
    server.send(400, "text/plain", "Keine gueltige Sicherungsdatei");
    return;
  }
  int n = 0;
  // Nur bekannte Schluessel uebernehmen, Zahlen auf sinnvolle Bereiche begrenzen
  #define RS_INT(key, feld, lo, hi)  if (!doc[key].isNull()) { cfg.feld = rsKlemme(doc[key].as<int>(), lo, hi); n++; }
  #define RS_BOOL(key, feld)         if (!doc[key].isNull()) { cfg.feld = doc[key].as<bool>(); n++; }
  RS_INT ("dwd_region",       dwd_region,       0, 255);
  RS_BOOL("pollen_warn",      pollen_warn);
  RS_BOOL("hinweis_regen",    hinweis_regen);
  RS_BOOL("hinweis_frost",    hinweis_frost);
  RS_BOOL("hinweis_uv",       hinweis_uv);
  RS_BOOL("hinweis_wind",     hinweis_wind);
  RS_INT ("main_box",         main_box,         0, 1);
  RS_BOOL("verlauf_regen",    verlauf_regen);
  RS_INT ("pollen_schwelle",  pollen_schwelle,  0, 3);
  RS_INT ("pollen_auswahl",   pollen_auswahl,   0, 255);
  RS_BOOL("warn_region",      warn_region);
  RS_INT ("dim_timeout",      dim_timeout,      -1, 10);
  RS_INT ("brightness",       brightness,       10, 100);
  RS_BOOL("night_mode",       night_mode);
  RS_INT ("night_from",       night_from,       0, 1439);
  RS_INT ("night_to",         night_to,         0, 1439);
  RS_INT ("night_brightness", night_brightness, 0, 50);
  RS_INT ("ampel_gruen_min",  ampel_gruen_min,  -50, 120);
  RS_INT ("ampel_gruen_max",  ampel_gruen_max,  -50, 120);
  RS_INT ("ampel_gelb_min",   ampel_gelb_min,   -50, 120);
  RS_INT ("ampel_gelb_max",   ampel_gelb_max,   -50, 120);
  RS_INT ("ampel_rot_min",    ampel_rot_min,    -50, 120);
  RS_INT ("ampel_rot_max",    ampel_rot_max,    -50, 120);
  RS_INT ("ampel_modus",      ampel_modus,      0, 2);
  RS_INT ("ws_cat0",          ws_cat0,          0, 6);
  RS_INT ("ws_cat1",          ws_cat1,          0, 6);
  RS_INT ("ws_cat2",          ws_cat2,          0, 6);
  RS_INT ("ws_cat3",          ws_cat3,          0, 6);
  #undef RS_INT
  #undef RS_BOOL
  String zone = String((const char*)(doc["bio_zone"] | ""));
  if (zone.length() == 1 && zone[0] >= 'A' && zone[0] <= 'K') { cfg.bio_zone = zone; n++; }
  String ort = String((const char*)(doc["location"] | ""));
  if (ort.length() > 0 && ort.length() <= 60) {
    cfg.location = ort; n++;
    float la = doc["lat"] | 0.0f, lo = doc["lon"] | 0.0f;
    bool koordOk = (la != 0.0f || lo != 0.0f) && la >= -90.0f && la <= 90.0f && lo >= -180.0f && lo <= 180.0f;
    cfg.lat = koordOk ? la : 0.0f;   // 0 = beim naechsten Start per Geocoding neu bestimmen
    cfg.lon = koordOk ? lo : 0.0f;
  }
  speichereCfg();
  logDiag("[Sicherung] %d Einstellungen wiederhergestellt, Neustart", n);
  server.send(200, "text/plain", "ok " + String(n));
  delay(1500);
  ESP.restart();
}

void handleApiAmpel() {
  letzteAmpelAnfrage = millis();

  float t = wetter.temp;
  const char* active; const char* modus; String detail;
  ampelBerechnen(active, modus, detail);

  bool dwd_warn = (anzahl_warnungen > 0 && !dwdWarnBestaetigt);

  // Diagnose: wie alt ist wetter.temp? (letzterWetterFetchErfolg==0 → noch nie erfolgreich seit Boot)
  long dataAgeMin = (letzterWetterFetchErfolg == 0)
                       ? -1
                       : (long)((millis() - letzterWetterFetchErfolg) / 60000UL);

  String json = "{";
  json += "\"temperature\":"  + String(t, 1) + ",";
  json += "\"data_age_min\":" + String(dataAgeMin) + ",";
  json += "\"dwd_warning\":"  + String(dwd_warn ? "true" : "false") + ",";
  json += "\"active\":\""     + String(active) + "\",";
  json += "\"mode\":\""       + String(modus) + "\",";
  json += "\"detail\":\""     + detail + "\",";
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
  if (server.hasArg("amp_modus"))  cfg.ampel_modus     = constrain(server.arg("amp_modus").toInt(), 0, 2);
  speichereCfg();
  server.sendHeader("Location", "/"); server.send(302);
}

// Diagnose-Log per Browser abrufbar (http://<ip>/log) — neuester Eintrag zuerst.
// RAM-Ringpuffer, überlebt keinen Neustart, aber genau dafür gedacht: Fehler ansehen
// SOLANGE das Gerät noch läuft, ohne USB/Serial vor Ort zu brauchen.
void handleApiLog() {
  String out = "WetterCubePlus Diagnose-Log (neueste zuerst)\n";
  out += "Aktueller freier Heap: " + String(ESP.getFreeHeap()) + " Bytes (tiefster Stand seit Start: " + String(ESP.getMinFreeHeap()) + ")\n";
  out += "PSRAM: frei " + String(ESP.getFreePsram()) + " von " + String(ESP.getPsramSize()) +
         " Bytes (tiefster Stand seit Start: " + String(ESP.getMinFreePsram()) + ", groesster Block " + String(ESP.getMaxAllocPsram()) + ")\n";
  out += "Uptime: " + String(millis() / 60000UL) + " Minuten\n";
  out += "LVGL-Speicher: " + lvglSpeicherText() + "\n";
  out += "Letzter Neustart: " + (resetGrundText.length() ? resetGrundText : String("-")) + "\n";
  out += "WLAN beim Start: " + (wifiStartInfo.length() ? wifiStartInfo : String("-")) + "\n";
  if (WiFi.status() == WL_CONNECTED)
    out += "WLAN jetzt: '" + WiFi.SSID() + "', Signal " + String(WiFi.RSSI()) + " dBm, IP " + WiFi.localIP().toString() + "\n";
  else
    out += "WLAN jetzt: nicht verbunden\n";
  out += "Standort: " + (cfg.location.length() ? cfg.location : String("-")) + " (" + String(cfg.lat, 4) + ", " + String(cfg.lon, 4) +
         (cfg.lat == 0.0f ? " = noch nicht gefunden" : "") + ")\n";
  out += "Letzter erfolgreicher Abruf:";
  for (int i = 0; i < AB_ANZAHL; i++) {
    out += String(i ? "," : "") + " " + AB_NAME[i] + " ";
    out += abrufErfolg[i] ? "vor " + String((millis() - abrufErfolg[i]) / 60000UL) + " Min." : String("noch nie");
  }
  out += "\n";
  out += "Update: installiert " + String(FIRMWARE_VERSION) + ", neueste bekannte " +
         (updateNeueVersion.length() ? updateNeueVersion : String("-")) +
         (updateVerfuegbar ? " (Update verfuegbar)" : "") + ", letzte Pruefung " +
         (letzterUpdateCheck ? "vor " + String((millis() - letzterUpdateCheck) / 60000UL) + " Min." : String("noch nicht")) + "\n";
  if (letzteAmpelAnfrage == 0) {
    out += "Letzte /api/ampel-Anfrage: noch keine seit dem letzten Neustart\n";
  } else {
    unsigned long sekundenHer = (millis() - letzteAmpelAnfrage) / 1000UL;
    out += "Letzte /api/ampel-Anfrage: vor " + String(sekundenHer) + "s (Poll-Intervall der Ampel: 30s — deutlich mehr heisst der Server war fuer sie nicht erreichbar)\n";
  }
  out += "----------------------------------------\n";
  if (logRingCount == 0) {
    out += "Noch keine Diagnose-Eintraege seit dem letzten Neustart.\n";
  } else {
    for (int i = 0; i < logRingCount; i++) {
      int idx = (logRingPos - 1 - i + LOG_RING_SIZE * 2) % LOG_RING_SIZE;
      out += String(logRing[idx]) + "\n";
    }
  }
  server.send(200, "text/plain; charset=utf-8", out);
}

// ============================================================
//  Firmware-Update (OTA): Versionspruefung, Hinweis-Screen mit Fortschritt, Installation
// ============================================================
void aktualisiereUpdateBadge();

// true, wenn Version a neuer ist als b ("1.1.0", "1.1.0-rc1"; ein Release ist neuer als dessen RC)
static bool versionNeuer(const String& a, const String& b) {
  int pa[3] = {0, 0, 0}, pb[3] = {0, 0, 0};
  sscanf(a.c_str(), "%d.%d.%d", &pa[0], &pa[1], &pa[2]);
  sscanf(b.c_str(), "%d.%d.%d", &pb[0], &pb[1], &pb[2]);
  for (int i = 0; i < 3; i++) if (pa[i] != pb[i]) return pa[i] > pb[i];
  return b.indexOf('-') >= 0 && a.indexOf('-') < 0;
}

// Fragt die neueste veroeffentlichte Version ab. true = Abfrage erfolgreich.
static bool holeNeuesteVersion(String& latest) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!zeitGueltig()) { logDiag("[OTA] Versionsabfrage uebersprungen: Uhrzeit noch nicht synchronisiert"); return false; }
  WiFiClientSecure sc; tlsSicher(sc);
  HTTPClient http;
  http.begin(sc, OTA_VERSION_URL);
  http.setTimeout(10000);
  bool ok = false;
  int httpCode = http.GET();
  if (httpCode != 200) {
    char tlsFehler[100] = "";
    sc.lastError(tlsFehler, sizeof(tlsFehler));
    logDiag("[OTA] Versionsabfrage fehlgeschlagen: HTTP %d %s", httpCode, tlsFehler);
  }
  if (httpCode == 200) {
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      const char* v = doc["version"] | "";
      if (v[0]) { latest = String(v); ok = true; }
    }
  }
  http.end();
  return ok;
}

// Automatische Pruefung (aus loop()): setzt updateVerfuegbar fuer das Badge auf Screen 1
static void pruefeUpdate() {
  letzterUpdateCheck = millis();
  String latest;
  if (!holeNeuesteVersion(latest)) {
    updateCheckIntervall = UPDATE_CHECK_RETRY;
    logDiag("[Update] Pruefung fehlgeschlagen, neuer Versuch in 1 Std.");
    return;
  }
  updateCheckIntervall = UPDATE_CHECK_INTERVALL;
  bool neu = versionNeuer(latest, FIRMWARE_VERSION);
  if (neu && (!updateVerfuegbar || updateNeueVersion != latest))
    logDiag("[Update] Neue Version verfuegbar: %s (installiert: %s)", latest.c_str(), FIRMWARE_VERSION);
  logDiag("[Update] Pruefung: installiert %s, verfuegbar %s", FIRMWARE_VERSION, latest.c_str());
  updateVerfuegbar  = neu;
  updateNeueVersion = latest;
}

// ---- Hinweis-Screen waehrend des Updates ----
static lv_obj_t* updScreen     = nullptr;
static lv_obj_t* updTitel      = nullptr;
static lv_obj_t* updText       = nullptr;
static lv_obj_t* updBalken     = nullptr;
static lv_obj_t* updProzent    = nullptr;
static int       updLetztesProzent = -1;

static void lvPump(uint32_t ms) {
  uint32_t t = millis();
  while (millis() - t < ms) { lv_timer_handler(); delay(5); }
}

// Kein Umlaut in den Texten: die LVGL-Built-in-Fonts kennen nur ASCII
static void zeigeUpdateScreen(const char* titel, uint32_t titelFarbe, const char* text, bool mitBalken) {
  if (!updScreen) {
    updScreen = lv_obj_create(nullptr);
    lv_obj_set_size(updScreen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(updScreen, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(updScreen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(updScreen, LV_OBJ_FLAG_SCROLLABLE);

    updTitel = lv_label_create(updScreen);
    lv_obj_set_style_text_font(updTitel, &lv_font_montserrat_20, 0);
    lv_obj_align(updTitel, LV_ALIGN_TOP_MID, 0, 24);

    updText = lv_label_create(updScreen);
    lv_label_set_long_mode(updText, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(updText, LV_HOR_RES - 40);
    lv_obj_set_style_text_font(updText, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(updText, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_align(updText, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(updText, LV_ALIGN_TOP_MID, 0, 72);

    updBalken = lv_bar_create(updScreen);
    lv_obj_set_size(updBalken, 400, 18);
    lv_obj_align(updBalken, LV_ALIGN_BOTTOM_MID, 0, -64);
    lv_bar_set_range(updBalken, 0, 100);
    lv_obj_set_style_bg_color(updBalken, lv_color_hex(0x333355), LV_PART_MAIN);
    lv_obj_set_style_bg_color(updBalken, lv_color_hex(0x4cd964), LV_PART_INDICATOR);

    updProzent = lv_label_create(updScreen);
    lv_obj_set_style_text_font(updProzent, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(updProzent, lv_color_hex(0xffffff), 0);
    lv_obj_align(updProzent, LV_ALIGN_BOTTOM_MID, 0, -30);
  }
  lv_label_set_text(updTitel, titel);
  lv_obj_set_style_text_color(updTitel, lv_color_hex(titelFarbe), 0);
  lv_obj_align(updTitel, LV_ALIGN_TOP_MID, 0, 24);
  lv_label_set_text(updText, text);
  if (mitBalken) {
    lv_obj_clear_flag(updBalken, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(updProzent, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(updBalken, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(updProzent, LV_OBJ_FLAG_HIDDEN);
  }
  updLetztesProzent = -1;
  lv_bar_set_value(updBalken, 0, LV_ANIM_OFF);
  lv_label_set_text(updProzent, "0 %");
  lv_scr_load(updScreen);
  lvPump(150);
}

// Wird von Update.writeStream() nach jedem geschriebenen Block aufgerufen
static void updateFortschritt(size_t done, size_t total) {
  if (!updBalken || total == 0) return;
  int p = (int)((uint64_t)done * 100 / total);
  if (p == updLetztesProzent) return;
  updLetztesProzent = p;
  lv_bar_set_value(updBalken, p, LV_ANIM_OFF);
  lv_label_set_text_fmt(updProzent, "%d %%", p);
  lv_timer_handler();
}

// Laedt und installiert die Firmware. Rueckgabe: Fehlertext, leer = Erfolg.
static String installiereUpdate() {
  logDiag("[OTA] Lade %s", OTA_BIN_URL);
  zeigeUpdateScreen(LV_SYMBOL_DOWNLOAD "  Update wird installiert", 0xf5a623,
                    "Bitte das Geraet NICHT vom Strom trennen!\n\n"
                    "Das Update dauert ca. 1 Minute, danach startet das Geraet selbst neu.", true);

  if (!zeitGueltig()) { logDiag("[OTA] Abbruch: Uhrzeit noch nicht synchronisiert"); return "time not synced"; }
  WiFiClientSecure client; tlsSicher(client);
  HTTPClient http;
  http.begin(client, OTA_BIN_URL);
  http.setTimeout(30000);
  int code = http.GET();
  if (code != 200) {
    http.end();
    char tlsFehler[100] = "";
    client.lastError(tlsFehler, sizeof(tlsFehler));
    logDiag("[OTA] Fehler: HTTP %d %s", code, tlsFehler);
    return "download error " + String(code);
  }

  int contentLen = http.getSize();
  if (!Update.begin(contentLen > 0 ? contentLen : UPDATE_SIZE_UNKNOWN)) {
    http.end();
    logDiag("[OTA] Fehler: begin failed (%d Bytes)", contentLen);
    return "begin failed";
  }
  Update.onProgress(updateFortschritt);
  size_t written = Update.writeStream(*http.getStreamPtr());
  bool success = Update.end(true) && !Update.hasError();
  http.end();

  if (!success) {
    String e = Update.errorString();
    logDiag("[OTA] Fehler: %s", e.c_str());
    return e.length() ? e : String("unknown error");
  }
  logDiag("[OTA] %u Bytes geschrieben, Neustart", (unsigned)written);
  return "";
}

static void updateErfolgNeustart() {
  zeigeUpdateScreen(LV_SYMBOL_OK "  Update installiert", 0x4cd964, "Das Geraet startet jetzt neu ...", false);
  lvPump(1500);
  ESP.restart();
}

// Zeigt den Fehler kurz an und springt zurueck zum Hauptscreen
static void updateFehlerAnzeigen(const String& fehler) {
  String text = "Das Update ist fehlgeschlagen:\n" + fehler + "\n\nDas Geraet laeuft mit der alten Version weiter.";
  zeigeUpdateScreen(LV_SYMBOL_WARNING "  Update fehlgeschlagen", 0xff5555, text.c_str(), false);
  lvPump(6000);
  loadScreen(SCREEN_ID_SCREEN_1);
}

void handleWebOtaCheck() {
  String latest = FIRMWARE_VERSION;
  bool ok = holeNeuesteVersion(latest);
  bool update_available = ok && versionNeuer(latest, FIRMWARE_VERSION);
  if (ok) {
    updateVerfuegbar  = update_available;
    updateNeueVersion = latest;
    aktualisiereUpdateBadge();
  }
  String json = "{\"current\":\"" + String(FIRMWARE_VERSION) + "\",\"latest\":\"" +
                latest + "\",\"update_available\":" +
                (update_available ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleWebOtaDoUpdate() {
  String fehler = installiereUpdate();
  if (fehler.length() == 0) {
    server.send(200, "text/plain", "ok");
    updateErfolgNeustart();
  } else {
    server.send(500, "text/plain", fehler);
    updateFehlerAnzeigen(fehler);
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
  static const char* kopfNamen[] = {"Origin", "Authorization"};
  server.collectHeaders(kopfNamen, 2);   // damit die Herkunftspruefung den Origin-Header sieht
  server.on("/",               HTTP_GET, GESCHUETZT(handleWebRoot));
  server.on("/speichern",      HTTP_POST, GESCHUETZT(handleWebSave));
  server.on("/backup",         HTTP_GET, GESCHUETZT(handleWebBackup));
  server.on("/restore",        HTTP_POST, GESCHUETZT(handleWebRestore));
  server.on("/ota_check",      HTTP_GET, GESCHUETZT(handleWebOtaCheck));
  server.on("/ota_update",     HTTP_POST, GESCHUETZT(handleWebOtaDoUpdate));
  server.on("/wlan",           HTTP_GET, GESCHUETZT(handleWebWlanAendern));
  server.on("/wlan_speichern", HTTP_POST, GESCHUETZT(handleWebWlanSave));
  server.on("/api/ampel",      HTTP_GET, GESCHUETZT(handleApiAmpel));
  server.on("/api/ampel_save", HTTP_POST, GESCHUETZT(handleApiAmpelSave));
  server.on("/log",            HTTP_GET, GESCHUETZT(handleApiLog));
  server.on("/passwort_speichern",            HTTP_POST, GESCHUETZT(handlePwSpeichern));
  server.on("/passwort_vergessen",            HTTP_GET,  GESCHUETZT(handlePwVergessen));
  server.on("/passwort_vergessen/start",      HTTP_POST, GESCHUETZT(handlePwVergessenStart));
  server.on("/passwort_vergessen/loeschen",   HTTP_POST, GESCHUETZT(handlePwVergessenLoeschen));
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

// ── Eigene Allergene (Auswahl in der WebUI) ──────────────────
static const char* POLLEN_ANZEIGE[8] = {"Hasel", "Erle", "Esche", "Birke", "Gräser", "Roggen", "Beifuß", "Ambrosia"};
static bool pollenGewaehlt(int i) { return (cfg.pollen_auswahl >> i) & 1; }

// Wert eines Allergens (DWD-Skala 0-3) fuer Warnung und Anzeige: Open-Meteo (naechste Stunde), wo vorhanden,
// sonst der DWD-Tageswert (Hasel, Esche und Roggen liefert Open-Meteo nicht)
static float pollenWertWarnung(int i) {
  switch (i) {
    case 0:  return pollen.dwd_hasel;
    case 1:  return openMeteoToDwd(pollen.erle);
    case 2:  return pollen.dwd_esche;
    case 3:  return openMeteoToDwd(pollen.birke);
    case 4:  return openMeteoToDwd(pollen.graeser);
    case 5:  return pollen.dwd_roggen;
    case 6:  return openMeteoToDwd(pollen.beifuss);
    default: return openMeteoToDwd(pollen.ambrosia);
  }
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

// Temperaturfarben: kraeftig auf hellem Grund, etwas heller auf dunklem Grund (sonst zu wenig Kontrast)
lv_color_t tempColor(float t) {
  const bool dunkel = (active_theme_index == THEME_DARK);
  if (t < 8)   return lv_color_hex(dunkel ? 0x3FA0FF : 0x0A5CE0);  // blau
  if (t < 15)  return lv_color_hex(dunkel ? 0x1FD46A : 0x00A040);  // grün
  if (t < 24)  return lv_color_hex(dunkel ? 0xFFC41A : 0xEE9B00);  // gelb
  return              lv_color_hex(dunkel ? 0xFF4A2E : 0xE01E0A);   // rot
}

const lv_img_dsc_t* wmoZuImage(int wmo, bool day) {
  if (wmo == 0)   return day ? &day_clear             : &night_full_moon_clear;          // Klar
  if (wmo <= 2)   return day ? &day_partial_cloud     : &night_full_moon_partial_cloud;  // Teilweise bewölkt
  if (wmo == 3)   return &overcast;                                                      // Bedeckt
  if (wmo <= 48)  return wmo == 45 ? &mist : &fog;                                       // Nebel (45) / Reifnebel (48)
  if (wmo <= 55)  return &rain;                                                          // Nieselregen
  if (wmo <= 57)  return &sleet;                                                         // Gefrierender Nieselregen
  if (wmo <= 65)  return &rain;                                                          // Regen
  if (wmo <= 67)  return &sleet;                                                         // Gefrierender Regen
  if (wmo <= 77)  return &snow;                                                          // Schnee
  if (wmo <= 82)  return day ? &day_rain : &night_full_moon_rain;                        // Regenschauer
  if (wmo <= 86)  return day ? &day_snow : &night_full_moon_snow;                        // Schneeschauer
  return &rain_thunder;                                                                  // Gewitter
}

// Wetter-Icons liegen als 128px-Quelle vor und werden auf 83px skaliert (Zoom 256 = 100 %)
static void setWetterIcon(lv_obj_t* obj, const lv_img_dsc_t* dsc) {
  if (!obj || !dsc) return;
  lv_img_set_src(obj, dsc);
  // Darstellungsgroesse = Breite des Widgets (in PicoPixel einstellbar); sonst wuerde LVGL das Bild kacheln oder abschneiden
  int w = lv_obj_get_width(obj);
  lv_img_set_zoom(obj, (uint16_t)((w > 0 ? (unsigned long)w : 83UL) * 256UL / dsc->header.w));
}

const char* windRichtung(int deg) {
  const char* dirs[] = {"N","NO","O","SO","S","SW","W","NW"};
  return dirs[((deg + 22) % 360) / 45];
}

// -- Wetterdaten (Open-Meteo) --
// Rueckgabe: true = Daten aktualisiert. Bei einem Fehlschlag wird frueher erneut versucht (siehe loop), sonst blieben die Werte bis zu 10 Min. leer.
static bool fetchWetterEinmal() {
  bool ok = false;
  spur("Wetter: Abruf");
  if (WiFi.status() != WL_CONNECTED || cfg.lat == 0.0f) return false;
  WiFiClientSecure sc; tlsSicher(sc);
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast";
  url += "?latitude="  + String(cfg.lat, 4);
  url += "&longitude=" + String(cfg.lon, 4);
  url += "&current=temperature_2m,relative_humidity_2m,apparent_temperature";
  url += ",wind_speed_10m,wind_direction_10m,surface_pressure,weather_code,is_day";
  url += "&daily=uv_index_max,sunrise,sunset,temperature_2m_min,temperature_2m_max";
  url += "&hourly=temperature_2m,weather_code,wind_speed_10m,wind_direction_10m,uv_index,precipitation,wind_gusts_10m";
  url += "&minutely_15=precipitation&forecast_minutely_15=12";
  url += "&timezone=auto&forecast_days=2";
  http.begin(sc, url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    DynamicJsonDocument doc(16384);
    DeserializationError jsonErr = deserializeJson(doc, http.getString());
    if (jsonErr == DeserializationError::Ok) {
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
      // UV Max der nächsten 3 Stunden
      float uvMax = 0.0f;
      for (int i = 1; i <= 3; i++) {
        float v = doc["hourly"]["uv_index"][min(h + i, 47)].as<float>();
        if (v > uvMax) uvMax = v;
      }
      wetter.uv_next3h = uvMax;
      // UV-Hinweis (jetzt bis +3 h) und Frost (jetzt bis +12 h); fehlende Werte nicht als 0 werten
      float uvH = 0.0f;
      for (int i = 0; i <= 3; i++) {
        JsonVariantConst v = doc["hourly"]["uv_index"][min(h + i, 47)];
        if (!v.isNull() && v.as<float>() > uvH) uvH = v.as<float>();
      }
      wetter.uv_hinweis = uvH;
      // Sturmböen: staerkste Boe von jetzt bis +6 h, fehlende Werte nicht als 0 werten
      {
        float bMax = 0.0f; int bIdx = -1;
        for (int i = 0; i <= 6 && h + i < 48; i++) {
          JsonVariantConst v = doc["hourly"]["wind_gusts_10m"][h + i];
          if (v.isNull()) continue;
          float g = v.as<float>();
          if (g > bMax) bMax = g;
          if (bIdx < 0 && g >= 62.0f) bIdx = i;   // Beaufort 8 (Sturm) beginnt bei 62 km/h
        }
        wetter.boe_max   = bMax;
        wetter.boe_jetzt = (bIdx == 0);
        wetter.boe_ab    = "";
        if (bIdx > 0) {
          String bt = doc["hourly"]["time"][h + bIdx].as<String>();
          if (bt.length() >= 16) wetter.boe_ab = bt.substring(11, 16);
        }
      }
      float fMin = 99.0f; int frostIdx = -1;
      for (int i = 0; i <= 12 && h + i < 48; i++) {
        JsonVariantConst v = doc["hourly"]["temperature_2m"][h + i];
        if (v.isNull()) continue;
        float tv = v.as<float>();
        if (tv < fMin) fMin = tv;
        if (frostIdx < 0 && tv <= 0.0f) frostIdx = i;
      }
      // Temperaturverlauf (jetzt bis +24 h)
      {
        int n = 0;
        for (int i = 0; i <= 24 && h + i < 48; i++) {
          JsonVariantConst vt = doc["hourly"]["temperature_2m"][h + i];
          if (vt.isNull()) break;
          wetter.vl_temp[n]  = vt.as<float>();
          JsonVariantConst vr = doc["hourly"]["precipitation"][h + i];
          wetter.vl_regen[n] = vr.isNull() ? 0.0f : vr.as<float>();
          n++;
        }
        wetter.vl_n  = n;
        wetter.vl_h0 = h;
      }
      wetter.frost_min   = fMin;
      wetter.frost_jetzt = (frostIdx == 0);
      wetter.frost_ab    = "";
      if (frostIdx > 0) {
        String ft = doc["hourly"]["time"][h + frostIdx].as<String>();
        if (ft.length() >= 16) wetter.frost_ab = ft.substring(11, 16);
      }
      // Regen-Nowcast: 15-Min-Slots der naechsten 3 Stunden
      JsonArray ncT = doc["minutely_15"]["time"].as<JsonArray>();
      JsonArray ncP = doc["minutely_15"]["precipitation"].as<JsonArray>();
      int ncN = 0;
      for (size_t i = 0; i < ncT.size() && i < ncP.size() && ncN < WetterDaten::NC_MAX; i++) {
        int y, mo, d, hh, mi;
        if (sscanf(ncT[i] | "", "%d-%d-%dT%d:%d", &y, &mo, &d, &hh, &mi) != 5) continue;
        struct tm t = {};
        t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = hh; t.tm_min = mi; t.tm_isdst = -1;
        wetter.nc_start[ncN] = mktime(&t);   // Zeitzone ist per setenv/configTime bereits auf lokal gesetzt
        wetter.nc_mm[ncN]    = ncP[i].as<float>();
        ncN++;
      }
      wetter.nc_n = ncN;
      // Erholung nach vorherigem(n) Fehlschlag(en) protokollieren (>15 Min = mind. 1 verpasster 10-Min-Zyklus).
      // Erfolge selbst NICHT jedes Mal loggen — würde den 25er-Ringpuffer in ~4h überschreiben.
      if (letzterWetterFetchErfolg != 0 && millis() - letzterWetterFetchErfolg > 900000UL) {
        logDiag("[Wetter] Fetch erfolgreich nach Unterbrechung (freeHeap=%u)", (unsigned)ESP.getFreeHeap());
      }
      letzterWetterFetchErfolg = millis();
      abrufOk(AB_WETTER);
      ok = true;
    } else {
      logDiag("[Wetter] JSON-Parse fehlgeschlagen: %s (freeHeap=%u)", jsonErr.c_str(), (unsigned)ESP.getFreeHeap());
    }
  } else {
    String antwort = httpCode > 0 ? http.getString() : String();
    antwort.replace("\n", " ");
    logDiag("[Wetter] HTTP-Fetch fehlgeschlagen: Code %d (freeHeap=%u) %s", httpCode, (unsigned)ESP.getFreeHeap(), antwort.substring(0, 60).c_str());
  }
  http.end();
  return ok;
}

// Direkt nach dem Start antwortet der Server gelegentlich mit 503: ein zweiter Versuch nach 3 s holt die Daten meist sofort nach
bool fetchWetter() {
  if (fetchWetterEinmal()) return true;
  if (WiFi.status() != WL_CONNECTED) return false;
  delay(3000);
  return fetchWetterEinmal();
}

// -- DWD Pollen (Stufenwarnung) --
void fetchDwdPollen() {
  if (WiFi.status() != WL_CONNECTED) return;
  spur("DWD-Pollen: Abruf");
  WiFiClientSecure sc; tlsSicher(sc);
  HTTPClient http;
  http.begin(sc, "https://opendata.dwd.de/climate_environment/health/alerts/s31fg.json");
  int httpCode = http.GET();
  Serial.printf("[Pollen] HTTP %d, region=%d\n", httpCode, cfg.dwd_region);
  if (httpCode != 200) logDiag("[Pollen] DWD-Abruf fehlgeschlagen: HTTP %d", httpCode);
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
    if (err != DeserializationError::Ok) logDiag("[Pollen] DWD JSON-Fehler: %s", err.c_str());
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
      if (!found) { Serial.printf("[Pollen] Region %d nicht gefunden!\n", cfg.dwd_region); logDiag("[Pollen] DWD-Region %d nicht gefunden", cfg.dwd_region); }
      else abrufOk(AB_DWD_POLLEN);
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
  if (!cfg.warn_region) { anzahl_warnungen = 0; return; }   // ausgeschaltet: keine alten Warnungen stehen lassen (sonst kommt der Button beim naechsten Auffrischen zurueck)
  if (WiFi.status() != WL_CONNECTED) return;
  spur("DWD-Warnungen: Abruf");
  WiFiClientSecure sc; tlsSicher(sc);
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
  if (code != 200) logDiag("[Warnungen] DWD-Abruf fehlgeschlagen: HTTP %d", code);

  if (code == 200) {
    String body = http.getString();
    http.end();
    Serial.printf("[DWD-Warn] %u Bytes\n", body.length());
    // GeoJSON FeatureCollection – Feldnamen aus properties loggen um sie zu kennen
    DynamicJsonDocument doc(32768);
    DeserializationError err = deserializeJson(doc, body);
    body = String();
    Serial.printf("[DWD-Warn] parse: %s\n", err.c_str());
    if (err != DeserializationError::Ok && err != DeserializationError::NoMemory) logDiag("[Warnungen] JSON-Fehler: %s", err.c_str());
    if (err == DeserializationError::Ok || err == DeserializationError::NoMemory) {
      abrufOk(AB_WARNUNGEN);
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
  spur("Radar: Abruf");
  WiFiClientSecure sc; tlsSicher(sc);
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
  if (httpCode != 200) { logDiag("[Radar] Abruf fehlgeschlagen: HTTP %d", httpCode); http.end(); return; }
  unsigned long tStart = millis();
  String body = http.getString();
  http.end();
  size_t got = body.length();
  Serial.printf("[Radar] Download: %u Bytes in %lums\n", got, millis() - tStart);

  String ct = http.header("Content-Type");
  if (!ct.startsWith("image/png")) {
    Serial.printf("[Radar] Fehler (mit Grenzen): %s\n", body.substring(0, 200).c_str());
    // Fallback: nur Radar-Layer ohne Bundesland-Grenzen
    WiFiClientSecure sc2; tlsSicher(sc2);
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
    if (c2 != 200) { logDiag("[Radar] Abruf (ohne Grenzen) fehlgeschlagen: HTTP %d", c2); http2.end(); return; }
    body = http2.getString();
    ct   = http2.header("Content-Type");
    http2.end();
    got  = body.length();
    Serial.printf("[Radar] Fallback: %u Bytes, CT: %s\n", got, ct.c_str());
    if (!ct.startsWith("image/png")) { Serial.println("[Radar] Fallback fehlgeschlagen"); return; }
  }
  if (got < 512) { Serial.printf("[Radar] Zu wenig Daten: %u\n", got); logDiag("[Radar] zu wenig Daten: %u Bytes", (unsigned)got); return; }
  const uint8_t* pngBuf = (const uint8_t*)body.c_str();
  Serial.printf("[Radar] %u Bytes geladen, dekodiere PNG\n", got);

  // PNG → RGBA8888 (lodepng nutzt malloc; mit PSRAM als Heap-Erweiterung reicht der Speicher)
  unsigned char* rgba = nullptr;
  unsigned imgW = 0, imgH = 0;
  spur("Radar: PNG dekodieren");
  unsigned err = lodepng_decode32(&rgba, &imgW, &imgH, pngBuf, got);

  if (err || !rgba) {
    Serial.printf("[Radar] lodepng Fehler %u\n", err);
    logDiag("[Radar] PNG-Fehler %u (freeHeap=%u)", err, (unsigned)ESP.getFreeHeap());
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
    logDiag("[Radar] zu wenig PSRAM fuer das Bild");
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

  abrufOk(AB_RADAR);
  spur("Radar: Bild anzeigen");
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
static bool warnPopupNurSchliessen = false;
static void zeigeWarnPopup(lv_obj_t* parent, const char* titel, const char* text, bool nurSchliessen = false);

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
  zeigeWarnPopup(objects.screenwarnkarte2, titel.c_str(), text.c_str(), true);
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
  spur("Pollen/Luft: Abruf");
  WiFiClientSecure sc; tlsSicher(sc);
  HTTPClient http;
  String url = "https://air-quality-api.open-meteo.com/v1/air-quality";
  url += "?latitude="  + String(cfg.lat, 4);
  url += "&longitude=" + String(cfg.lon, 4);
  url += "&hourly=birch_pollen,grass_pollen,alder_pollen,mugwort_pollen,ragweed_pollen";
  url += ",european_aqi,pm2_5,pm10,nitrogen_dioxide,ozone";
  url += "&timezone=auto&forecast_days=1";
  http.begin(sc, url);
  int omCode = http.GET();
  if (omCode == 200) {
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
      abrufOk(AB_POLLEN_OM);
    } else {
      logDiag("[Pollen] Open-Meteo: JSON-Fehler (freeHeap=%u)", (unsigned)ESP.getFreeHeap());
    }
  } else {
    logDiag("[Pollen] Open-Meteo-Abruf fehlgeschlagen: HTTP %d (freeHeap=%u)", omCode, (unsigned)ESP.getFreeHeap());
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
  spur("Biowetter: Abruf");
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[Bio] Kein WiFi"); return; }
  Serial.printf("[Bio] Lade Zone %s...\n", cfg.bio_zone.c_str());
  WiFiClientSecure sc; tlsSicher(sc);
  HTTPClient http;
  http.begin(sc, "https://opendata.dwd.de/climate_environment/health/alerts/biowetter.json");
  http.setTimeout(15000);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[Bio] HTTP %d\n", code);
    logDiag("[Biowetter] DWD-Abruf fehlgeschlagen: HTTP %d", code);
    http.end(); return;
  }
  int contentLen = http.getSize();
  Serial.printf("[Bio] HTTP 200, %d Bytes\n", contentLen);

  // Ganzen Body lesen (110KB liegt im PSRAM)
  String body = http.getString();
  http.end();
  Serial.printf("[Bio] Body geladen: %d Bytes\n", body.length());
  if (body.length() < 1000) { Serial.println("[Bio] Body zu kurz"); logDiag("[Biowetter] Antwort zu kurz: %u Bytes", (unsigned)body.length()); return; }

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

  DynamicJsonDocument doc(32768);
  DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err != DeserializationError::Ok) {
    Serial.printf("[Bio] JSON Fehler: %s\n", err.c_str());
    logDiag("[Biowetter] JSON-Fehler: %s (freeHeap=%u)", err.c_str(), (unsigned)ESP.getFreeHeap());
    return;
  }

  JsonArray zones = doc["zone"];
  Serial.printf("[Bio] %d Zonen geparst\n", (int)zones.size());
  for (JsonObject z : zones) {
    String id = z["id"].as<String>();
    if (id != cfg.bio_zone) continue;
    for (int p = 0; p < 4; p++) {
      JsonArray eff = z[perioden[p]]["effect"];
      bio.gueltig[p] = ((int)eff.size() >= 7);
      Serial.printf("[Bio] Periode %d (%s): %d Eintraege\n", p, perioden[p], (int)eff.size());
      for (int k = 0; k < 7 && k < (int)eff.size(); k++) {
        const char* val = eff[k]["value"].as<const char*>();
        bio.wert[p][k] = bioWertToInt(val);
        Serial.printf("  [%d]=%d (%s)\n", k, bio.wert[p][k], val ? val : "null");
      }
    }
    bio.geladen = true;
    break;
  }
  if (!bio.geladen) { Serial.printf("[Bio] Zone %s nicht gefunden\n", cfg.bio_zone.c_str()); logDiag("[Biowetter] Zone %s nicht gefunden", cfg.bio_zone.c_str()); }
  else abrufOk(AB_BIO);
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
  for (int i = 0; i < 8; i++) {
    if (!pollenGewaehlt(i)) continue;
    float v = pollenWertWarnung(i);
    if (v > max_v) { max_v = v; name = POLLEN_ANZEIGE[i]; }
  }
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

// Einzelwerte – Farbkodierung nach EU-Grenzwerten. Eine einzige gemeinsame Definition
// für screenairquality UND screenhealth, damit ein Wert dort nie unterschiedlich
// eingefärbt werden kann (vorher: zwei getrennte, abweichende Skalen).
static lv_color_t pm25Color(float v) {
  if (v < 0)   return lv_color_hex(0x888888);
  if (v <= 10) return lv_color_hex(0x50c878);
  if (v <= 25) return lv_color_hex(0xffd700);
  if (v <= 50) return lv_color_hex(0xff8c00);
  return lv_color_hex(0xff3030);
}
static lv_color_t pm10Color(float v) {
  if (v < 0)    return lv_color_hex(0x888888);
  if (v <= 20)  return lv_color_hex(0x50c878);
  if (v <= 50)  return lv_color_hex(0xffd700);
  if (v <= 100) return lv_color_hex(0xff8c00);
  return lv_color_hex(0xff3030);
}
static lv_color_t no2Color(float v) {
  if (v < 0)    return lv_color_hex(0x888888);
  if (v <= 40)  return lv_color_hex(0x50c878);
  if (v <= 100) return lv_color_hex(0xffd700);
  if (v <= 200) return lv_color_hex(0xff8c00);
  return lv_color_hex(0xff3030);
}
static lv_color_t o3Color(float v) {
  if (v < 0)     return lv_color_hex(0x888888);
  if (v <= 60)   return lv_color_hex(0x50c878);
  if (v <= 120)  return lv_color_hex(0xffd700);
  if (v <= 180)  return lv_color_hex(0xff8c00);
  return lv_color_hex(0xff3030);
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

// Ampelfarbe je nach Modus (Temperatur / Pollen / Luftqualitaet); detail = Klartext fuer die WebUI
static void ampelBerechnen(const char*& active, const char*& modus, String& detail) {
  float t = wetter.temp;
  active = "none";
  modus  = "temperature";
  detail = String(t, 1) + " °C";
  if (cfg.ampel_modus == 1) {
    // Pollen: schlimmster der gewaehlten Allergene (Skala 0-3), Stufen wie pollenText(): Gruen = keine/gering (<= 1), Gelb = mittel/hoch (<= 2,5), Rot = stark (> 2,5)
    modus = "pollen";
    float v = -1.0f; const char* name = "";
    for (int i = 0; i < 8; i++) {
      if (!pollenGewaehlt(i)) continue;
      float w = pollenWertWarnung(i);
      if (w > v) { v = w; name = POLLEN_ANZEIGE[i]; }
    }
    if (v >= 0.0f) {
      active = (v > 2.5f) ? "red" : (v > 1.0f) ? "yellow" : "green";
      detail = String(name) + " (Stufe " + String(v, 1) + " von 3)";
    } else detail = "keine Pollendaten";
  } else if (cfg.ampel_modus == 2) {
    // Luftqualitaet: Europaeischer AQI, Gruen bis 40 (gut/mittel), Gelb bis 60 (maessig), Rot darueber
    modus = "air";
    int a = pollen.aqi;
    if (a >= 0) {
      active = (a <= 40) ? "green" : (a <= 60) ? "yellow" : "red";
      detail = "AQI " + String(a) + " (" + aqiStatusText(a) + ")";
    } else detail = "keine Luftqualitaetsdaten";
  } else {
    if      (t >= cfg.ampel_rot_min)   active = "red";
    else if (t >= cfg.ampel_gelb_min)  active = "yellow";
    else if (t >= cfg.ampel_gruen_min) active = "green";
  }

}


// ── Hinweis-Kachel auf Screen 1 (Regen / Frost / UV) ─────────────────────────
// Neuer Hinweis: Kachel blinkt, Antippen bestaetigt (danach Dauerleuchten, solange die Bedingung gilt).
// Mehrere Hinweise: unbestaetigte zuerst (Regen, Frost, UV), danach Dauerleuchten des wichtigsten (gleiche Reihenfolge);
// Antippen im Dauerleuchten schaltet zum naechsten aktiven Hinweis. Ein bestaetigter Hinweis blinkt erst wieder,
// wenn seine Bedingung mindestens 20 Minuten lang weg war.
enum { HW_REGEN = 0, HW_WIND = 1, HW_FROST = 2, HW_UV = 3, HW_ANZAHL = 4 };   // Reihenfolge = Vorrang
struct Hinweis { bool aktiv = false; bool bestaetigt = false; unsigned long inaktivSeit = 0; char text[48] = ""; };
static Hinweis hinweise[HW_ANZAHL];
static const uint32_t HW_FARBE[HW_ANZAHL] = {0x0A5CE0, 0x5B21B6, 0x0E7490, 0xB45309};   // Blau, Violett, Petrol, Orangebraun (weisse Schrift)
static const float    HW_WIND_SCHWELLE = 62.0f;                                          // km/h, Beaufort 8 (Sturm)
static const float    HW_UV_SCHWELLE = 6.0f;                                  // UV-Index "hoch"
static int  hinweisAngezeigt = -1;
static int  hinweisManuell   = -1;
static bool hinweisBlinkt    = false;
static bool hinweisPhase     = false;

// Regen-Nowcast: "Regen in 25 Min." / "Regen noch 10 Min." / "Es regnet"
static bool hinweisRegen(char* buf, size_t n) {
  const float SCHWELLE = 0.1f;   // mm je 15 Min. (ab hier zaehlt ein Slot als Regen)
  time_t jetzt = time(nullptr);
  if (wetter.nc_n <= 0 || jetzt < 1700000000L || jetzt - wetter.nc_start[0] >= 6 * 3600L) return false;
  int aktuell = -1;   // Slot, in dem wir gerade sind
  for (int i = 0; i < wetter.nc_n; i++) {
    if (jetzt >= wetter.nc_start[i] && jetzt < wetter.nc_start[i] + 900) { aktuell = i; break; }
  }
  if (aktuell >= 0 && wetter.nc_mm[aktuell] >= SCHWELLE) {
    int ende = -1;   // erster trockener Slot danach
    for (int i = aktuell + 1; i < wetter.nc_n; i++) if (wetter.nc_mm[i] < SCHWELLE) { ende = i; break; }
    if (ende < 0) { snprintf(buf, n, "Es regnet"); return true; }
    int min = (int)((wetter.nc_start[ende] - jetzt + 59) / 60);
    snprintf(buf, n, "Regen noch %d Min.", min < 1 ? 1 : min);
    return true;
  }
  for (int i = 0; i < wetter.nc_n; i++) {
    if (wetter.nc_start[i] + 900 <= jetzt) continue;   // Slot schon vorbei
    if (wetter.nc_mm[i] >= SCHWELLE) {
      int min = (int)((wetter.nc_start[i] - jetzt + 59) / 60);
      snprintf(buf, n, "Regen in %d Min.", min < 1 ? 1 : min);
      return true;
    }
  }
  return false;
}

// Frost: tiefste Temperatur der naechsten 12 Stunden <= 0 Grad
static bool hinweisFrost(char* buf, size_t n) {
  if (wetter.frost_min > 0.0f) return false;   // auch 99 = keine Daten
  int t = (int)floorf(wetter.frost_min + 0.5f);
  if (wetter.frost_jetzt)               snprintf(buf, n, "Frost, bis %d °C", t);
  else if (wetter.frost_ab.length())    snprintf(buf, n, "Frost ab %s, %d °C", wetter.frost_ab.c_str(), t);
  else                                  snprintf(buf, n, "Frost, %d °C", t);
  return true;
}

// Sturm: Boeen ab 62 km/h in den naechsten 6 Stunden (die Kachel nutzt die PicoPixel-Schrift montserrat_14 mit Umlauten)
static bool hinweisWind(char* buf, size_t n) {
  if (wetter.boe_max < HW_WIND_SCHWELLE) return false;
  int v = (int)floorf(wetter.boe_max + 0.5f);
  if (wetter.boe_jetzt || !wetter.boe_ab.length()) snprintf(buf, n, "Sturmböen bis %d km/h", v);
  else                                              snprintf(buf, n, "Sturmböen ab %s, %d km/h", wetter.boe_ab.c_str(), v);
  return true;
}

// UV: Index jetzt oder in den naechsten 3 Stunden ab der Schwelle
static bool hinweisUv(char* buf, size_t n) {
  if (wetter.uv_hinweis < HW_UV_SCHWELLE) return false;
  int uv = (int)floorf(wetter.uv_hinweis + 0.5f);
  const char* stufe = wetter.uv_hinweis >= 11.0f ? "extrem" : wetter.uv_hinweis >= 8.0f ? "sehr hoch" : "hoch";
  snprintf(buf, n, "UV %s (%d), Sonnenschutz", stufe, uv);
  return true;
}

static void zeichneHinweisFarbe() {
  if (!objects.labelregen || hinweisAngezeigt < 0) return;
  lv_color_t c = lv_color_hex(HW_FARBE[hinweisAngezeigt]);
  if (hinweisBlinkt && !hinweisPhase) c = lv_color_mix(lv_color_black(), c, 120);   // dunklere Phase beim Blinken
  lv_obj_set_style_bg_color(objects.labelregen, c, LV_PART_MAIN);
}

static void aktualisiereHinweise() {
  if (!objects.labelregen) return;
  char buf[HW_ANZAHL][48];
  bool akt[HW_ANZAHL];
  akt[HW_REGEN] = cfg.hinweis_regen && hinweisRegen(buf[HW_REGEN], sizeof(buf[0]));
  akt[HW_WIND]  = cfg.hinweis_wind  && hinweisWind (buf[HW_WIND],  sizeof(buf[0]));
  akt[HW_FROST] = cfg.hinweis_frost && hinweisFrost(buf[HW_FROST], sizeof(buf[0]));
  akt[HW_UV]    = cfg.hinweis_uv    && hinweisUv   (buf[HW_UV],    sizeof(buf[0]));
  unsigned long jetzt = millis();
  for (int i = 0; i < HW_ANZAHL; i++) {
    Hinweis& h = hinweise[i];
    if (akt[i]) {
      strlcpy(h.text, buf[i], sizeof(h.text));
      h.aktiv = true;
      h.inaktivSeit = 0;
    } else {
      if (h.aktiv) { h.aktiv = false; h.inaktivSeit = jetzt ? jetzt : 1; }
      if (h.bestaetigt && h.inaktivSeit && jetzt - h.inaktivSeit >= 1200000UL) { h.bestaetigt = false; h.inaktivSeit = 0; }
    }
  }
  int zeige = -1;
  hinweisBlinkt = false;
  for (int i = 0; i < HW_ANZAHL; i++) if (hinweise[i].aktiv && !hinweise[i].bestaetigt) { zeige = i; hinweisBlinkt = true; break; }
  if (zeige < 0) {
    if (hinweisManuell >= 0 && hinweise[hinweisManuell].aktiv) zeige = hinweisManuell;
    else for (int i = 0; i < HW_ANZAHL; i++) if (hinweise[i].aktiv) { zeige = i; break; }
  }
  hinweisAngezeigt = zeige;
  if (zeige < 0) {
    lv_obj_add_flag(objects.labelregen, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  // Farbige Kachel mit weisser Schrift
  lv_obj_set_style_bg_opa(objects.labelregen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_color(objects.labelregen, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_radius(objects.labelregen, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(objects.labelregen, 5, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(objects.labelregen, 6, LV_PART_MAIN);
  lv_obj_set_height(objects.labelregen, LV_SIZE_CONTENT);
  lv_label_set_text(objects.labelregen, hinweise[zeige].text);
  zeichneHinweisFarbe();
  lv_obj_clear_flag(objects.labelregen, LV_OBJ_FLAG_HIDDEN);
}

static void cbHinweisBlink(lv_timer_t*) {
  hinweisPhase = !hinweisPhase;
  if (hinweisBlinkt) zeichneHinweisFarbe();
}

static void cbHinweisTap(lv_event_t*) {
  if (hinweisAngezeigt < 0) return;
  if (hinweisBlinkt) {
    hinweise[hinweisAngezeigt].bestaetigt = true;   // bestaetigen: naechster unbestaetigter Hinweis oder Dauerleuchten
    hinweisManuell = -1;
  } else {
    for (int k = 1; k < HW_ANZAHL; k++) {            // Dauerleuchten: zum naechsten aktiven Hinweis wechseln
      int c = (hinweisAngezeigt + k) % HW_ANZAHL;
      if (hinweise[c].aktiv) { hinweisManuell = c; break; }
    }
  }
  aktualisiereHinweise();
}

// ── Temperaturverlauf (screenverlauf): Liniendiagramm 24 h + optionale Regenbalken ──
static lv_chart_series_t* vlSerT = nullptr;
static lv_chart_series_t* vlSerR = nullptr;
static lv_coord_t vlT[25];
static lv_coord_t vlR[25];
static lv_obj_t*  vlRegenInfo = nullptr;
static bool vlInit = false;

static int rundeInt(float v) { return (int)floorf(v + 0.5f); }

// Beschriftung der Achsen: Temperatur und Regen sind mit 10 multipliziert (Zehntel), Zeit als Uhrzeit
static void verlaufDrawCb(lv_event_t* e) {
  lv_obj_draw_part_dsc_t* d = (lv_obj_draw_part_dsc_t*)lv_event_get_param(e);
  if (!lv_obj_draw_part_check_type(d, &lv_chart_class, LV_CHART_DRAW_PART_TICK_LABEL)) return;
  if (!d->text) return;
  if (d->id == LV_CHART_AXIS_PRIMARY_X) {
    lv_snprintf(d->text, d->text_length, "%02d", (wetter.vl_h0 + (int)d->value * 4) % 24);   // Marke alle 4 Stunden
  } else if (d->id == LV_CHART_AXIS_PRIMARY_Y) {
    int v = (int)d->value;
    lv_snprintf(d->text, d->text_length, "%d", (v < 0 ? v - 5 : v + 5) / 10);
  }
}

static void verlaufAchse(lv_obj_t* ch, bool mitZeit, int yMarken) {
  lv_chart_set_axis_tick(ch, LV_CHART_AXIS_PRIMARY_Y, 4, 0, yMarken, 1, true, 30);
  if (mitZeit) lv_chart_set_axis_tick(ch, LV_CHART_AXIS_PRIMARY_X, 4, 0, 7, 1, true, 24);
  else         lv_chart_set_axis_tick(ch, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0, false, 0);
}

static void aktualisiereVerlauf() {
  spur("UI: Temperaturverlauf");
  lv_obj_t* tc = objects.chartverlauf;
  lv_obj_t* rc = objects.chartregen;
  if (!tc || !rc) return;
  lv_color_t txt   = lv_color_hex(theme_colors[active_theme_index][THEME_COLOR_TEXT]);
  lv_color_t gitter = lv_color_mix(txt, lv_color_hex(theme_colors[active_theme_index][THEME_COLOR_BACKGR_CONT]), 40);
  bool dunkel = (active_theme_index == THEME_DARK);
  if (!vlInit) {
    vlInit = true;
    for (int i = 0; i < 25; i++) { vlT[i] = LV_CHART_POINT_NONE; vlR[i] = LV_CHART_POINT_NONE; }
    vlSerT = lv_chart_add_series(tc, lv_color_hex(0xE5572B), LV_CHART_AXIS_PRIMARY_Y);
    vlSerR = lv_chart_add_series(rc, lv_color_hex(0x0A5CE0), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_point_count(tc, 25);
    lv_chart_set_point_count(rc, 25);
    lv_chart_set_ext_y_array(tc, vlSerT, vlT);
    lv_chart_set_ext_y_array(rc, vlSerR, vlR);
    lv_obj_t* cs[2] = {tc, rc};
    for (int k = 0; k < 2; k++) {
      lv_obj_set_style_bg_opa(cs[k], LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(cs[k], 0, LV_PART_MAIN);
      lv_obj_set_style_pad_left(cs[k], 8, LV_PART_MAIN);     // Halber Balkenabstand: Linienpunkte stehen mittig ueber den Balken
      lv_obj_set_style_pad_right(cs[k], 8, LV_PART_MAIN);
      lv_obj_set_style_line_width(cs[k], 1, LV_PART_MAIN);
      lv_obj_set_style_size(cs[k], 0, LV_PART_INDICATOR);     // keine Punktmarken
      lv_obj_add_event_cb(cs[k], verlaufDrawCb, LV_EVENT_DRAW_PART_BEGIN, nullptr);
    }
    lv_obj_set_style_line_width(tc, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_column(rc, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(rc, 2, LV_PART_ITEMS);
    vlRegenInfo = lv_label_create(objects.containerverlauf);
    lv_obj_set_pos(vlRegenInfo, 38, 246);
  }
  // Farben je Farbthema
  lv_obj_t* cs[2] = {tc, rc};
  for (int k = 0; k < 2; k++) {
    lv_obj_set_style_line_color(cs[k], gitter, LV_PART_MAIN);
    lv_obj_set_style_text_color(cs[k], txt, LV_PART_TICKS);
    lv_obj_set_style_line_color(cs[k], txt, LV_PART_TICKS);
  }
  vlSerR->color = lv_color_hex(dunkel ? 0x3FA0FF : 0x0A5CE0);
  vlSerT->color = lv_color_hex(dunkel ? 0xFF7A4D : 0xE5572B);
  lv_obj_set_style_text_color(vlRegenInfo, txt, 0);

  // Layout: mit Regenbalken ist das Temperaturdiagramm kuerzer, die Zeitachse haengt unter den Balken
  bool mitRegen = cfg.verlauf_regen;
  if (mitRegen) { lv_obj_clear_flag(rc, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(vlRegenInfo, LV_OBJ_FLAG_HIDDEN); lv_obj_set_height(tc, 136); }
  else          { lv_obj_add_flag(rc, LV_OBJ_FLAG_HIDDEN);   lv_obj_add_flag(vlRegenInfo, LV_OBJ_FLAG_HIDDEN);   lv_obj_set_height(tc, 190); }
  verlaufAchse(tc, !mitRegen, 5);
  verlaufAchse(rc, mitRegen, 2);

  int n = wetter.vl_n;
  if (n < 2) {
    for (int i = 0; i < 25; i++) { vlT[i] = LV_CHART_POINT_NONE; vlR[i] = LV_CHART_POINT_NONE; }
    if (objects.labelverlaufminmax) lv_label_set_text(objects.labelverlaufminmax, "keine Daten");
    lv_label_set_text(vlRegenInfo, "");
    lv_chart_refresh(tc); lv_chart_refresh(rc);
    return;
  }
  float tmin = wetter.vl_temp[0], tmax = wetter.vl_temp[0], rmax = 0, rsum = 0;
  for (int i = 0; i < n; i++) {
    if (wetter.vl_temp[i] < tmin) tmin = wetter.vl_temp[i];
    if (wetter.vl_temp[i] > tmax) tmax = wetter.vl_temp[i];
    if (wetter.vl_regen[i] > rmax) rmax = wetter.vl_regen[i];
    rsum += wetter.vl_regen[i];
  }
  // Temperatur-Skala in ganzen Grad, Spanne durch 4 teilbar (5 Beschriftungen mit ganzen Zahlen)
  int lo = (int)floorf(tmin) - 1;
  int hi = (int)ceilf(tmax) + 1;
  int span = hi - lo; if (span < 4) span = 4;
  span = ((span + 3) / 4) * 4;
  hi = lo + span;
  lv_chart_set_range(tc, LV_CHART_AXIS_PRIMARY_Y, lo * 10, hi * 10);
  lv_chart_set_div_line_count(tc, 3, 5);
  int rtop = (int)ceilf(rmax); if (rtop < 1) rtop = 1;
  lv_chart_set_range(rc, LV_CHART_AXIS_PRIMARY_Y, 0, rtop * 10);
  lv_chart_set_div_line_count(rc, 2, 5);   // mindestens 2 waagerechte Linien: LVGL teilt durch (Anzahl - 1)
  for (int i = 0; i < 25; i++) {
    if (i < n) { vlT[i] = (lv_coord_t)rundeInt(wetter.vl_temp[i] * 10.0f); vlR[i] = (lv_coord_t)rundeInt(wetter.vl_regen[i] * 10.0f); }
    else       { vlT[i] = LV_CHART_POINT_NONE; vlR[i] = LV_CHART_POINT_NONE; }
  }
  lv_chart_refresh(tc);
  lv_chart_refresh(rc);
  char buf[64];
  if (objects.labelverlaufminmax) {
    snprintf(buf, sizeof(buf), "Min %d °C   Max %d °C", rundeInt(tmin), rundeInt(tmax));
    lv_label_set_text(objects.labelverlaufminmax, buf);
  }
  if (rsum < 0.05f) snprintf(buf, sizeof(buf), "Regen: keiner erwartet");
  else              snprintf(buf, sizeof(buf), "Regen: bis %.1f mm/h, zusammen %.1f mm", rmax, rsum);
  lv_label_set_text(vlRegenInfo, buf);
}

// Rechtes Feld auf Screen 1: die zwei staerksten Pollen unter "Meine Allergene" oder die zwei staerksten Biowetter-Werte
// der als Favoriten gewaehlten Kategorien (gleiche Auswahl wie auf dem Gesundheitsscreen); Umschalter: WebUI und Menue
// Die dritte Titelzeile hat die Groesse ihres Inhalts (linke Kante fest): nach einem Textwechsel mittig im Feld ausrichten
static void mainBoxTitel3Zentrieren() {
  lv_obj_t* o = objects.labelmainboxtitle3;
  if (!o) return;
  lv_obj_t* par = lv_obj_get_parent(o);
  lv_obj_update_layout(o);
  lv_obj_set_x(o, (lv_obj_get_width(par) - lv_obj_get_width(o)) / 2);
}

static void aktualisiereMainBox() {
  spur("UI: Hauptscreen-Feld");
  static const char* const BIO_KAT[7] = {"Allg. Befinden","Hypotonie","Bluthochdruck","Rheuma entz.","Rheuma deg.","Asthma","Waermebelast."};
  lv_obj_t* pn[] = {objects.labelpollenforecast1main,      objects.labelpollenforecast2main};
  lv_obj_t* pv[] = {objects.labelpollenforecast1mainvalue, objects.labelpollenforecast2mainvalue};
  if (cfg.main_box == 1) {
    struct tm ti; getLocalTime(&ti);
    int periode = (ti.tm_hour >= 18) ? 1 : 0;   // wie Gesundheitsscreen: bis 18 Uhr heute Nachmittag, danach morgen Vormittag
    // drei Titelzeilen: "Biowetter" / "heute" / "Nachmittag" (eine Zeile "heute Nachmittag" waere zu breit und wuerde abgeschnitten)
    setLabel(objects.labelmainboxtitle1, "Biowetter");
    setLabel(objects.labelmainboxtitle2, periode == 0 ? "heute" : "morgen");
    setLabel(objects.labelmainboxtitle3, periode == 0 ? "Nachmittag" : "Vormittag");
    mainBoxTitel3Zentrieren();
    bool leer = !bio.geladen || !bio.gueltig[periode];
    int cats[4] = {cfg.ws_cat0, cfg.ws_cat1, cfg.ws_cat2, cfg.ws_cat3};
    struct BE { int rang; int v; int cat; };
    BE be[4];
    for (int i = 0; i < 4; i++) {
      int cat = (cats[i] < 0 || cats[i] > 6) ? 0 : cats[i];
      int v = leer ? 0 : bio.wert[periode][cat];
      be[i] = { v == 3 ? 3 : v == 2 ? 2 : v == 0 ? 1 : 0, v, cat };   // hoch > gering > kein > positiv
    }
    for (int i = 0; i < 3; i++)
      for (int j = i + 1; j < 4; j++)
        if (be[j].rang > be[i].rang) { BE t = be[i]; be[i] = be[j]; be[j] = t; }
    for (int i = 0; i < 2; i++) {
      setLabel(pn[i], BIO_KAT[be[i].cat]);
      if (leer) setLabelFmt(pv[i], lv_color_hex(0x444444), "-");
      else      setLabelFmt(pv[i], bioWertColor(be[i].v), bioWertKurz(be[i].v));
    }
    return;
  }
  setLabel(objects.labelmainboxtitle1, "Pollenbelastung");
  setLabel(objects.labelmainboxtitle2, "in der nächsten");
  setLabel(objects.labelmainboxtitle3, "Stunde");
  mainBoxTitel3Zentrieren();
  struct PE { float v; const char* n; };
  PE pe[8];
  int pn_ = 0;
  for (int i = 0; i < 8; i++)
    if (pollenGewaehlt(i)) pe[pn_++] = {pollenWertWarnung(i), POLLEN_ANZEIGE[i]};
  for (int i = 0; i < pn_ - 1; i++)
    for (int j = i + 1; j < pn_; j++)
      if (pe[j].v > pe[i].v) { PE tmp = pe[i]; pe[i] = pe[j]; pe[j] = tmp; }
  for (int i = 0; i < 2; i++) {
    float v = (i < pn_) ? pe[i].v : -1.0f;   // weniger als zwei gewaehlt: "--"
    setLabel(pn[i], v >= 0 ? pe[i].n : "--");
    setPollenLabel(pv[i], v);
  }
}

void aktualisiereUI() {
  spur("UI: aktualisieren");
  aktualisiereHinweise();
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
  if (objects.imagewetter) setWetterIcon(objects.imagewetter, wmoZuImage(wetter.wmo_code, wetter.is_day));
  if (objects.labelversion) lv_label_set_text(objects.labelversion, FIRMWARE_VERSION);   // die Datenquelle steht im eigenen Label labelquellemenue

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

  aktualisiereMainBox();
  aktualisiereVerlauf();

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
    if (himg[i]) setWetterIcon(himg[i], wmoZuImage(wetter.wmo_forecast[i], true));
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
  // Nur die in der WebUI gewaehlten Allergene; Open-Meteo naechste Stunde (auf DWD-Skala umgerechnet),
  // fuer Hasel, Esche und Roggen der DWD-Tageswert
  float maxPollen = -1.0f;
  for (int pi = 0; pi < 8; pi++) if (pollenGewaehlt(pi)) maxPollen = max(maxPollen, pollenWertWarnung(pi));
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
      lv_label_set_text(objects.labelaqivalue, buf);
    }
    if (objects.labelaqistatus)
      lv_label_set_text(objects.labelaqistatus, aqiStatusText(aqi));

    // Einzelwerte – aktuelle Stunde, Farbkodierung über die gemeinsamen Funktionen
    // pm25Color/pm10Color/no2Color/o3Color (oben) – identisch zu screenhealth.
    auto setAqiBar = [](lv_obj_t* bar, float val, int rangeMax, lv_color_t col) {
      if (!bar) return;
      lv_bar_set_range(bar, 0, rangeMax);
      lv_bar_set_value(bar, (val >= 0) ? (int)val : 0, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(bar, col, LV_PART_INDICATOR);
    };

    if (objects.labelpm25value) {
      snprintf(buf, sizeof(buf), (pollen.pm25 >= 0) ? "%.1f" : "--", pollen.pm25);
      setLabelFmt(objects.labelpm25value, pm25Color(pollen.pm25), buf);
    }
    setAqiBar(objects.barlabelpm25value, pollen.pm25, 75, pm25Color(pollen.pm25));

    if (objects.labelpm10value) {
      snprintf(buf, sizeof(buf), (pollen.pm10 >= 0) ? "%.1f" : "--", pollen.pm10);
      setLabelFmt(objects.labelpm10value, pm10Color(pollen.pm10), buf);
    }
    setAqiBar(objects.barpm10value, pollen.pm10, 150, pm10Color(pollen.pm10));

    if (objects.labelno2value) {
      snprintf(buf, sizeof(buf), (pollen.no2 >= 0) ? "%.0f" : "--", pollen.no2);
      setLabelFmt(objects.labelno2value, no2Color(pollen.no2), buf);
    }
    setAqiBar(objects.barlabelno2value, pollen.no2, 200, no2Color(pollen.no2));

    if (objects.labelo3value) {
      snprintf(buf, sizeof(buf), (pollen.o3 >= 0) ? "%.0f" : "--", pollen.o3);
      setLabelFmt(objects.labelo3value, o3Color(pollen.o3), buf);
    }
    setAqiBar(objects.barlabelo3value, pollen.o3, 240, o3Color(pollen.o3));
  }

  // ── Screenhealth ─────────────────────────────────────────────
  {
    // Uhrzeit + Temperatur
    if (objects.labelwstime && hasTime) {
      strftime(buf, sizeof(buf), "%H:%M", &ti);
      lv_label_set_text(objects.labelwstime, buf);
    }
    if (objects.labeldatum_1 && hasTime) {
      const char* wt[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
      snprintf(buf, sizeof(buf), "%s. %02d.%02d.%04d",
        wt[ti.tm_wday], ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
      lv_label_set_text(objects.labeldatum_1, buf);
    }
    if (objects.labelwstemp) {
      snprintf(buf, sizeof(buf), "%.0f°C", wetter.temp);
      setLabelFmt(objects.labelwstemp, tempColor(wetter.temp), buf);
    }

    // Pollen: Top-4 nach DWD-Tageswert (heute), nur die in der WebUI gewaehlten Allergene
    struct PolEntry { float val; const char* name; int idx; };
    PolEntry polAll[] = {
      {pollen.dwd_birke,    "Birke",    3},
      {pollen.dwd_erle,     "Erle",     1},
      {pollen.dwd_esche,    "Esche",    2},
      {pollen.dwd_hasel,    "Hasel",    0},
      {pollen.dwd_graeser,  "Graeser",  4},
      {pollen.dwd_roggen,   "Roggen",   5},
      {pollen.dwd_beifuss,  "Beifuss",  6},
      {pollen.dwd_ambrosia, "Ambrosia", 7},
    };
    for (auto& e : polAll) if (!pollenGewaehlt(e.idx)) e.val = -100.0f;   // nicht gewaehlt: ganz nach hinten
    // Sortiere absteigend nach Wert (einfaches Selection-Sort, nur 8 Einträge)
    for (int i = 0; i < 7; i++)
      for (int j = i + 1; j < 8; j++)
        if (polAll[j].val > polAll[i].val) { PolEntry tmp = polAll[i]; polAll[i] = polAll[j]; polAll[j] = tmp; }

    lv_obj_t* polNameLabels[4] = {objects.labelwspol1name, objects.labelwspol2name, objects.labelwspol3name, objects.labelwspol4name};
    lv_obj_t* polValLabels[4]  = {objects.labelwspol1val,  objects.labelwspol2val,  objects.labelwspol3val,  objects.labelwspol4val};
    for (int i = 0; i < 4; i++) {
      float v = polAll[i].val;
      if (v <= -99.0f) {   // weniger als 4 Allergene gewaehlt: Platz leer lassen
        if (polNameLabels[i]) lv_label_set_text(polNameLabels[i], "-");
        if (polValLabels[i])  lv_label_set_text(polValLabels[i], "");
        continue;
      }
      if (polNameLabels[i]) lv_label_set_text(polNameLabels[i], polAll[i].name);
      if (polValLabels[i])  setPollenLabel(polValLabels[i], v);
    }

    // Wetterfühligkeit: 4 konfigurierbare Kategorien
    // Periode zeitabhängig: < 18 Uhr → heute Nachmittag (0), ab 18 Uhr → morgen Vormittag (1)
    static const char* BIO_NAMEN[] = {"Allg. Befinden","Hypotonie","Bluthochdruck","Rheuma entz.","Rheuma deg.","Asthma","Waermebelast."};
    int wsCats[4] = {cfg.ws_cat0, cfg.ws_cat1, cfg.ws_cat2, cfg.ws_cat3};
    lv_obj_t* catNameLabels[4] = {objects.labelwscat1name, objects.labelwscat2name, objects.labelwscat3name, objects.labelwscat4name};
    lv_obj_t* catValLabels[4]  = {objects.labelwscat1val,  objects.labelwscat2val,  objects.labelwscat3val,  objects.labelwscat4val};
    struct tm tiNow; getLocalTime(&tiNow);
    int bioPeriode = (tiNow.tm_hour >= 18) ? 1 : 0;
    // Statt statischem "Bio Wetter"-Titel den angezeigten Zeitraum selbst zeigen –
    // macht sichtbar, welche der 4 Perioden von screenbiowetter hier gerade zu sehen ist.
    if (objects.biowetter) {
      lv_obj_set_style_text_font(objects.biowetter, &font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_label_set_text(objects.biowetter, bioPeriode == 0 ? "Heute Nachmittag" : "Morgen Vormittag");
    }
    bool bioVerfuegbar = bio.geladen;
    bool bioLeer = !bioVerfuegbar || !bio.gueltig[bioPeriode];   // "kein Einfluss" in allen Kategorien ist ein gueltiger Wert
    for (int i = 0; i < 4; i++) {
      int cat = wsCats[i];
      if (cat < 0 || cat > 6) cat = 0;
      if (catNameLabels[i]) lv_label_set_text(catNameLabels[i], BIO_NAMEN[cat]);
      if (catValLabels[i]) {
        if (!bioVerfuegbar || bioLeer) {
          setLabelFmt(catValLabels[i], lv_color_hex(0x444444), "-");
        } else {
          int v = bio.wert[bioPeriode][cat];
          setLabelFmt(catValLabels[i], bioWertColor(v), bioWertKurz(v));
        }
      }
    }

    // Footer: AQI + O3 + UV
    // Aktuelle Stunde + gemeinsame Farbfunktionen (oben) – identisch zu screenairquality,
    // damit ein Wert auf beiden Screens garantiert dieselbe Farbe zeigt.
    auto setDot = [](lv_obj_t* obj, lv_color_t col) {
      if (!obj) return;
      lv_obj_set_style_bg_color(obj, col, 0);
    };
    setDot(objects.labelwsaqi,  pollen.aqi  < 0 ? lv_color_hex(0x555555) : aqiColor(pollen.aqi));
    setDot(objects.labelwspm25, pollen.pm25 < 0 ? lv_color_hex(0x555555) : pm25Color(pollen.pm25));
    setDot(objects.labelwso3,   pollen.o3   < 0 ? lv_color_hex(0x555555) : o3Color(pollen.o3));
    if (objects.labelwsuv) {
      float uv = wetter.uv_next3h;
      lv_color_t uvc = (uv <= 0) ? lv_color_hex(0x555555) :
                       (uv >= 8) ? lv_color_hex(0xff3030) :
                       (uv >= 6) ? lv_color_hex(0xff8c00) :
                       (uv >= 3) ? lv_color_hex(0xffd700) : lv_color_hex(0x50c878);
      setDot(objects.labelwsuv, uvc);
    }
  }

  // ── Biowetter (screenbiowetter + screenbiowetter2) ──────────
  // Screen 1 (Heute):  Vormittag = keine Daten (der DWD liefert fuer heute nur den Nachmittag), Nachmittag = Periode 0
  // Screen 2 (Morgen): Vormittag = Periode 1, Nachmittag = Periode 2
  // Perioden: 0 = heute Nachmittag, 1 = morgen Vormittag, 2 = morgen Nachmittag, 3 = uebermorgen Vormittag (nicht angezeigt)
  // Kategorien: 0=Befinden 1=BP-niedrig 2=BP-hoch 3=Rheuma-entz 4=Rheuma-degen 5=Asthma 6=Wärme
  {
    // Label-Arrays für beide Screens: [screen][periode_in_screen][kategorie]
    lv_obj_t* vmLabels[2][7] = {
      // Screen 1 – Vormittag (keine Daten)
      { objects.labelbiobevindenvm,   objects.labelbiobpniedrigvm,  objects.labelbiobphochvm,
        objects.labelbioreumentzvm,   objects.labelbioreumdegenvm,  objects.labelbioapsthmvm,
        objects.labelbiow_rmevm },
      // Screen 2 – Periode 1 (morgen Vormittag)
      { objects.labelbiobevindenvm2,  objects.labelbiobpniedrigvm2, objects.labelbiobphochvm2,
        objects.labelbioreumentzvm2,  objects.labelbioreumdegenvm2, objects.labelbioapsthmvm2,
        objects.labelbiow_rmevm2 }
    };
    lv_obj_t* nmLabels[2][7] = {
      // Screen 1 – Periode 0 (heute Nachmittag)
      { objects.labelbiobewindennm,   objects.labelbiobpniedrignm,  objects.labelbiobphochnm,
        objects.labelbioreumentznm,   objects.labelbioreumdegenm,   objects.labelbioapsthmam,
        objects.labelbiow_rmenm },
      // Screen 2 – Periode 2 (morgen Nachmittag)
      { objects.labelbiobewindennm2,  objects.labelbiobpniedrignm2, objects.labelbiobphochnm2,
        objects.labelbioreumentznm2,  objects.labelbioreumdegenm2,  objects.labelbioapsthmam2,
        objects.labelbiow_rmenm2 }
    };
    // Perioden-Indices im bio.wert[p][k] Array
    const int vmPeriod[2] = {-1, 1};
    const int nmPeriod[2] = {0, 2};

    for (int s = 0; s < 2; s++) {
      // Keine Daten: Periode fehlt (-1) oder der DWD hat sie nicht geliefert; "kein Einfluss" in allen Kategorien ist ein gueltiger Wert
      bool vmLeer = !bio.geladen || vmPeriod[s] < 0 || !bio.gueltig[vmPeriod[s]];
      bool nmLeer = !bio.geladen || nmPeriod[s] < 0 || !bio.gueltig[nmPeriod[s]];

      for (int k = 0; k < 7; k++) {
        if (vmLabels[s][k]) {
          int v = (!vmLeer) ? bio.wert[vmPeriod[s]][k] : 0;
          setLabelFmt(vmLabels[s][k], vmLeer ? lv_color_hex(0x444444) : bioWertColor(v),
                      vmLeer ? "-" : bioWertKurz(v));
        }
        if (nmLabels[s][k]) {
          int v = (!nmLeer) ? bio.wert[nmPeriod[s]][k] : 0;
          setLabelFmt(nmLabels[s][k], nmLeer ? lv_color_hex(0x444444) : bioWertColor(v),
                      nmLeer ? "-" : bioWertKurz(v));
        }
      }
    }
  }
}

// ============================================================
//  Mini-Spiel: Bubblebreaker (screenmenu → buttonbreaker)
// ============================================================
#define BB_COLS   10
#define BB_ROWS   7
#define BB_CELL_W 48
#define BB_CELL_H 40
#define BB_TOP    34
#define BB_CANVAS_W 480
#define BB_CANVAS_H (BB_ROWS * BB_CELL_H)
static const uint32_t BB_COLORS[5] = { 0xff4d4d, 0xc94dff, 0xffe14d, 0x4dff88, 0x4dc9ff };

enum BBModus { BB_MODUS_NORMAL, BB_MODUS_LAUFEND };
static BBModus bbModus = BB_MODUS_NORMAL;
static lv_obj_t* bbModusNormalBtn = nullptr;
static lv_obj_t* bbModusLaufendBtn = nullptr;

static lv_obj_t*   bbScreen = nullptr;
static lv_obj_t*   breakerSelectScreen = nullptr;
static lv_obj_t*   bbHsRows[5];
// Spielfeld als einzelne lv_canvas (statt vieler Einzelobjekte) – Pixelpuffer liegt im
// PSRAM und belastet damit NICHT den knappen internen LVGL-Speicherpool (LV_MEM_SIZE).
static lv_obj_t*   bbCanvas = nullptr;
static lv_color_t* bbCanvasBuf = nullptr;
static int8_t    bbBoard[BB_ROWS][BB_COLS];   // -1 = leer, 0..4 = Farbindex
static bool      bbBesucht[BB_ROWS][BB_COLS];
static int8_t    bbBoardUndo[BB_ROWS][BB_COLS];
static int       bbScoreUndo = 0;
static bool      bbUndoVerfuegbar = false;
static lv_obj_t* bbUndoBtn = nullptr;
static lv_obj_t* bbLblScore = nullptr;
static lv_obj_t* bbLblBeste = nullptr;
static lv_obj_t* bbGameOverPanel = nullptr;
static lv_obj_t* bbLblGameOverTitel = nullptr;
static lv_obj_t* bbLblGameOverScore = nullptr;
static int  bbScore = 0;
static bool bbAktiv = false;

static int32_t bbHighscores[5];
static uint8_t bbHighscoreCount = 0;

// ── Highscore-Persistenz (NVS, eigener Namespace) ────────────────────────────
static void bbLadeHighscores() {
  Preferences prefs; prefs.begin("wcp_bbreak", true);
  bbHighscoreCount = prefs.getUChar("cnt", 0);
  char key[4];
  for (int i = 0; i < 5; i++) {
    snprintf(key, sizeof(key), "s%d", i);
    bbHighscores[i] = prefs.getInt(key, 0);
  }
  prefs.end();
}

static void bbSpeichereHighscores() {
  Preferences prefs; prefs.begin("wcp_bbreak", false);
  prefs.putUChar("cnt", bbHighscoreCount);
  char key[4];
  for (int i = 0; i < 5; i++) {
    snprintf(key, sizeof(key), "s%d", i);
    prefs.putInt(key, bbHighscores[i]);
  }
  prefs.end();
}

static bool bbFuegeHighscoreEin(int32_t score) {
  if (bbHighscoreCount < 5) {
    bbHighscores[bbHighscoreCount++] = score;
  } else if (score > bbHighscores[4]) {
    bbHighscores[4] = score;
  } else {
    return false;
  }
  for (int i = bbHighscoreCount - 1; i > 0 && bbHighscores[i] > bbHighscores[i - 1]; i--) {
    int32_t tmp = bbHighscores[i]; bbHighscores[i] = bbHighscores[i - 1]; bbHighscores[i - 1] = tmp;
  }
  bbSpeichereHighscores();
  return true;
}

static void bbAktualisiereHighscoreAnzeige() {
  for (int i = 0; i < 5; i++) {
    if (i < bbHighscoreCount)
      lv_label_set_text_fmt(bbHsRows[i], "%d. %ld", i + 1, (long)bbHighscores[i]);
    else
      lv_label_set_text_fmt(bbHsRows[i], "%d. --", i + 1);
  }
}

// ── Brett-Logik ───────────────────────────────────────────────────────────────
static void bbNeuesBrett() {
  for (int r = 0; r < BB_ROWS; r++)
    for (int c = 0; c < BB_COLS; c++)
      bbBoard[r][c] = (int8_t)random(5);
}

// Zeichnet das komplette Brett auf die eine Canvas-Fläche (PSRAM-Puffer).
static void bbZeichneBrett() {
  if (!bbCanvas) return;
  lv_canvas_fill_bg(bbCanvas, lv_color_white(), LV_OPA_COVER);

  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.radius = LV_RADIUS_CIRCLE;
  dsc.bg_opa = LV_OPA_COVER;

  int size = BB_CELL_H - 4;
  int xOff = (BB_CELL_W - BB_CELL_H) / 2 + 2;

  for (int r = 0; r < BB_ROWS; r++) {
    for (int c = 0; c < BB_COLS; c++) {
      if (bbBoard[r][c] < 0) continue;
      dsc.bg_color = lv_color_hex(BB_COLORS[bbBoard[r][c]]);
      lv_canvas_draw_rect(bbCanvas, c * BB_CELL_W + xOff, r * BB_CELL_H + 2, size, size, &dsc);
    }
  }
}

// Iterative Flood-Fill (kein Rekursions-Stack-Risiko auf dem ESP32)
static void bbFloodFill(int startR, int startC, int8_t farbe, int* rs, int* cs, int* n) {
  static int stackR[BB_ROWS * BB_COLS];
  static int stackC[BB_ROWS * BB_COLS];
  int sp = 0;
  stackR[sp] = startR; stackC[sp] = startC; sp++;
  bbBesucht[startR][startC] = true;
  const int dr[4] = { -1, 1, 0, 0 };
  const int dc[4] = { 0, 0, -1, 1 };
  while (sp > 0) {
    sp--;
    int r = stackR[sp], c = stackC[sp];
    rs[*n] = r; cs[*n] = c; (*n)++;
    for (int k = 0; k < 4; k++) {
      int nr = r + dr[k], nc = c + dc[k];
      if (nr < 0 || nr >= BB_ROWS || nc < 0 || nc >= BB_COLS) continue;
      if (bbBesucht[nr][nc] || bbBoard[nr][nc] != farbe) continue;
      bbBesucht[nr][nc] = true;
      stackR[sp] = nr; stackC[sp] = nc; sp++;
    }
  }
}

// Schwerkraft (Bubbles fallen in ihrer Spalte nach unten) + Spalten-Kollaps (leere Spalten → links auffüllen)
static void bbSchwerkraftUndKollaps() {
  for (int c = 0; c < BB_COLS; c++) {
    int writeR = BB_ROWS - 1;
    for (int r = BB_ROWS - 1; r >= 0; r--) {
      if (bbBoard[r][c] >= 0) {
        if (writeR != r) { bbBoard[writeR][c] = bbBoard[r][c]; bbBoard[r][c] = -1; }
        writeR--;
      }
    }
  }
  int writeC = 0;
  for (int c = 0; c < BB_COLS; c++) {
    bool leer = true;
    for (int r = 0; r < BB_ROWS; r++) if (bbBoard[r][c] >= 0) { leer = false; break; }
    if (!leer) {
      if (writeC != c) {
        for (int r = 0; r < BB_ROWS; r++) { bbBoard[r][writeC] = bbBoard[r][c]; bbBoard[r][c] = -1; }
      }
      writeC++;
    }
  }
}

// Modus "Laufend": leer gewordene Spalten (nach Schwerkraft/Kollaps) rechts mit frischen
// Zufallsbubbles auffüllen, statt sie leer zu lassen – Spielfeld bleibt dauerhaft voll.
static void bbFuelleAuf() {
  for (int c = 0; c < BB_COLS; c++) {
    bool leer = true;
    for (int r = 0; r < BB_ROWS; r++) if (bbBoard[r][c] >= 0) { leer = false; break; }
    if (leer) {
      for (int r = 0; r < BB_ROWS; r++) bbBoard[r][c] = (int8_t)random(5);
    }
  }
}

static bool bbHatZuege() {
  for (int r = 0; r < BB_ROWS; r++) {
    for (int c = 0; c < BB_COLS; c++) {
      if (bbBoard[r][c] < 0) continue;
      if (c + 1 < BB_COLS && bbBoard[r][c + 1] == bbBoard[r][c]) return true;
      if (r + 1 < BB_ROWS && bbBoard[r + 1][c] == bbBoard[r][c]) return true;
    }
  }
  return false;
}

static bool bbIstLeer() {
  for (int r = 0; r < BB_ROWS; r++)
    for (int c = 0; c < BB_COLS; c++)
      if (bbBoard[r][c] >= 0) return false;
  return true;
}

static void bbGameUeber() {
  bbAktiv = false;
  bool neuerHighscore = bbFuegeHighscoreEin(bbScore);
  int32_t beste = bbHighscoreCount > 0 ? bbHighscores[0] : 0;
  lv_label_set_text(bbLblGameOverTitel, "Spiel zu Ende");
  lv_label_set_text_fmt(bbLblGameOverScore, "Punkte: %d%s\nBeste: %ld",
                         bbScore, neuerHighscore ? "  (Highscore!)" : "", (long)beste);
  lv_obj_clear_flag(bbGameOverPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(bbGameOverPanel);
}

static int bbFillRs[BB_ROWS * BB_COLS];
static int bbFillCs[BB_ROWS * BB_COLS];

static void cbBubbleTap(lv_event_t* e) {
  if (!bbAktiv) return;
  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  lv_area_t canvasCoords;
  lv_obj_get_coords(bbCanvas, &canvasCoords);
  int lx = p.x - canvasCoords.x1;
  int ly = p.y - canvasCoords.y1;
  if (lx < 0 || ly < 0 || lx >= BB_CANVAS_W || ly >= BB_CANVAS_H) return;
  int c = lx / BB_CELL_W;
  int r = ly / BB_CELL_H;
  if (r < 0 || r >= BB_ROWS || c < 0 || c >= BB_COLS) return;
  if (bbBoard[r][c] < 0) return;
  memset(bbBesucht, 0, sizeof(bbBesucht));
  int n = 0;
  bbFloodFill(r, c, bbBoard[r][c], bbFillRs, bbFillCs, &n);
  if (n < 2) return;

  // Undo-Schnappschuss vor der Änderung sichern (ein Schritt zurück möglich)
  memcpy(bbBoardUndo, bbBoard, sizeof(bbBoard));
  bbScoreUndo = bbScore;
  bbUndoVerfuegbar = true;
  if (bbUndoBtn) lv_obj_set_style_bg_color(bbUndoBtn, lv_color_hex(0x2980b9), 0);

  for (int i = 0; i < n; i++) bbBoard[bbFillRs[i]][bbFillCs[i]] = -1;
  bbScore += (n - 1) * (n - 1) * 5;
  lv_label_set_text_fmt(bbLblScore, "Punkte: %d", bbScore);
  bbSchwerkraftUndKollaps();
  if (bbModus == BB_MODUS_LAUFEND) bbFuelleAuf();
  bbZeichneBrett();
  if (bbModus == BB_MODUS_NORMAL && bbIstLeer()) {
    bbScore += 500;
    lv_label_set_text_fmt(bbLblScore, "Punkte: %d", bbScore);
    bbGameUeber();
    return;
  }
  if (!bbHatZuege()) bbGameUeber();
}

static void cbBBUndoTap(lv_event_t*) {
  if (!bbAktiv || !bbUndoVerfuegbar) return;
  memcpy(bbBoard, bbBoardUndo, sizeof(bbBoard));
  bbScore = bbScoreUndo;
  bbUndoVerfuegbar = false;
  if (bbUndoBtn) lv_obj_set_style_bg_color(bbUndoBtn, lv_color_hex(0xbdc3c7), 0);
  lv_label_set_text_fmt(bbLblScore, "Punkte: %d", bbScore);
  bbZeichneBrett();
}

static void bbStarteSpiel() {
  bbScore = 0;
  lv_label_set_text(bbLblScore, "Punkte: 0");
  int32_t beste = bbHighscoreCount > 0 ? bbHighscores[0] : 0;
  lv_label_set_text_fmt(bbLblBeste, "Beste: %ld", (long)beste);
  lv_obj_add_flag(bbGameOverPanel, LV_OBJ_FLAG_HIDDEN);
  bbUndoVerfuegbar = false;
  if (bbUndoBtn) lv_obj_set_style_bg_color(bbUndoBtn, lv_color_hex(0xbdc3c7), 0);
  bbNeuesBrett();
  bbZeichneBrett();
  bbAktiv = true;
  lv_scr_load(bbScreen);
}

static void cbBreakerStartTap(lv_event_t*) { bbStarteSpiel(); }

static void cbBBExitTap(lv_event_t*) {
  bbAktiv = false;
  loadScreen(SCREEN_ID_SCREEN_1);
}

static void zeigeBreakerSelect() {
  bbLadeHighscores();
  bbAktualisiereHighscoreAnzeige();
  lv_scr_load(breakerSelectScreen);
}

static void cbOpenBreakerSelect(lv_event_t*) { zeigeBreakerSelect(); }

static void bbAktualisiereModusButtons() {
  if (bbModusNormalBtn)
    lv_obj_set_style_bg_color(bbModusNormalBtn, bbModus == BB_MODUS_NORMAL ? lv_color_hex(0x2c3e50) : lv_color_hex(0xbdc3c7), 0);
  if (bbModusLaufendBtn)
    lv_obj_set_style_bg_color(bbModusLaufendBtn, bbModus == BB_MODUS_LAUFEND ? lv_color_hex(0x2c3e50) : lv_color_hex(0xbdc3c7), 0);
}
static void cbBBModusNormal(lv_event_t*)  { bbModus = BB_MODUS_NORMAL;  bbAktualisiereModusButtons(); }
static void cbBBModusLaufend(lv_event_t*) { bbModus = BB_MODUS_LAUFEND; bbAktualisiereModusButtons(); }

static void cbBBNochmalTap(lv_event_t*) { bbStarteSpiel(); }
static void cbBBZurueckMenuTap(lv_event_t*) {
  bbAktiv = false;
  zeigeBreakerSelect();
}

static lv_obj_t* erstelleTextButton(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                                     uint32_t bgColor, const char* text, uint32_t textColor = 0xffffff) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bgColor), 0);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_color(lbl, lv_color_hex(textColor), 0);
  lv_obj_center(lbl);
  return btn;
}

// Eigenständiger Auswahl-Screen: Bestenliste, Start, Zurück.
static void erstelleBreakerSelectScreen() {
  breakerSelectScreen = lv_obj_create(nullptr);
  lv_obj_set_size(breakerSelectScreen, 480, 320);
  lv_obj_set_style_bg_color(breakerSelectScreen, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(breakerSelectScreen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(breakerSelectScreen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* titel = lv_label_create(breakerSelectScreen);
  lv_label_set_text(titel, "Bubblebreaker");
  lv_obj_set_style_text_font(titel, &font_montserrat_22, 0);
  lv_obj_set_style_text_color(titel, lv_color_hex(0x1a1a1a), 0);
  lv_obj_align(titel, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t* hsTitel = lv_label_create(breakerSelectScreen);
  lv_label_set_text(hsTitel, "Bestenliste");
  lv_obj_set_style_text_font(hsTitel, &font_montserrat_18, 0);
  lv_obj_set_style_text_color(hsTitel, lv_color_hex(0x1a1a1a), 0);
  lv_obj_align(hsTitel, LV_ALIGN_TOP_MID, 0, 55);

  for (int i = 0; i < 5; i++) {
    bbHsRows[i] = lv_label_create(breakerSelectScreen);
    lv_obj_set_style_text_font(bbHsRows[i], &font_montserrat_18, 0);
    lv_obj_set_style_text_color(bbHsRows[i], lv_color_hex(0x333333), 0);
    lv_obj_align(bbHsRows[i], LV_ALIGN_TOP_MID, 0, 90 + i * 22);
    lv_label_set_text_fmt(bbHsRows[i], "%d. --", i + 1);
  }

  // Spielmodus-Auswahl
  bbModusNormalBtn = erstelleTextButton(breakerSelectScreen, 95, 208, 140, 36, 0x2c3e50, "Normal");
  lv_obj_add_event_cb(bbModusNormalBtn, cbBBModusNormal, LV_EVENT_CLICKED, nullptr);
  bbModusLaufendBtn = erstelleTextButton(breakerSelectScreen, 245, 208, 140, 36, 0xbdc3c7, "Laufend");
  lv_obj_add_event_cb(bbModusLaufendBtn, cbBBModusLaufend, LV_EVENT_CLICKED, nullptr);
  bbAktualisiereModusButtons();

  lv_obj_t* zurueckBtn = erstelleTextButton(breakerSelectScreen, 20, 255, 120, 45, 0x7f8c8d, "Zurueck");
  lv_obj_add_event_cb(zurueckBtn, [](lv_event_t*) { loadScreen(SCREEN_ID_SCREENMENU); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startBtn = erstelleTextButton(breakerSelectScreen, 300, 255, 160, 45, 0x2ecc71, "Start");
  lv_obj_add_event_cb(startBtn, cbBreakerStartTap, LV_EVENT_CLICKED, nullptr);
}

static void erstelleBubbleBreakerScreen() {
  bbScreen = lv_obj_create(nullptr);
  lv_obj_set_size(bbScreen, 480, 320);
  lv_obj_set_style_bg_color(bbScreen, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(bbScreen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(bbScreen, LV_OBJ_FLAG_SCROLLABLE);

  bbLblScore = lv_label_create(bbScreen);
  lv_label_set_text(bbLblScore, "Punkte: 0");
  lv_obj_set_style_text_font(bbLblScore, &font_montserrat_16, 0);
  lv_obj_set_style_text_color(bbLblScore, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_pos(bbLblScore, 10, 6);

  bbLblBeste = lv_label_create(bbScreen);
  lv_label_set_text(bbLblBeste, "Beste: 0");
  lv_obj_set_style_text_font(bbLblBeste, &font_montserrat_16, 0);
  lv_obj_set_style_text_color(bbLblBeste, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_pos(bbLblBeste, 200, 6);

  bbUndoBtn = lv_btn_create(bbScreen);
  lv_obj_set_pos(bbUndoBtn, 330, 2);
  lv_obj_set_size(bbUndoBtn, 75, 28);
  lv_obj_set_style_bg_color(bbUndoBtn, lv_color_hex(0xbdc3c7), 0);
  lv_obj_set_style_radius(bbUndoBtn, 6, 0);
  lv_obj_t* undoLbl = lv_label_create(bbUndoBtn);
  lv_label_set_text(undoLbl, "Undo");
  lv_obj_set_style_text_color(undoLbl, lv_color_white(), 0);
  lv_obj_center(undoLbl);
  lv_obj_add_event_cb(bbUndoBtn, cbBBUndoTap, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* bbExitBtn = lv_btn_create(bbScreen);
  lv_obj_set_pos(bbExitBtn, 430, 2);
  lv_obj_set_size(bbExitBtn, 44, 30);
  lv_obj_set_style_bg_color(bbExitBtn, lv_color_hex(0xcc0000), 0);
  lv_obj_set_style_radius(bbExitBtn, 6, 0);
  lv_obj_t* exitLbl = lv_label_create(bbExitBtn);
  lv_label_set_text(exitLbl, LV_SYMBOL_CLOSE);
  lv_obj_set_style_text_color(exitLbl, lv_color_white(), 0);
  lv_obj_center(exitLbl);
  lv_obj_add_event_cb(bbExitBtn, cbBBExitTap, LV_EVENT_CLICKED, nullptr);

  // Spielfeld: einzelne Canvas statt vieler Einzelobjekte – Puffer im PSRAM (8 MB frei),
  // belastet den knappen internen LVGL-Speicherpool nicht.
  uint32_t bbBufSize = LV_CANVAS_BUF_SIZE_TRUE_COLOR(BB_CANVAS_W, BB_CANVAS_H);
  bbCanvasBuf = (lv_color_t*)ps_malloc(bbBufSize);
  if (bbCanvasBuf) {
    bbCanvas = lv_canvas_create(bbScreen);
    lv_canvas_set_buffer(bbCanvas, bbCanvasBuf, BB_CANVAS_W, BB_CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(bbCanvas, 0, BB_TOP);
    lv_obj_clear_flag(bbCanvas, LV_OBJ_FLAG_SCROLLABLE);
    // WICHTIG: lv_canvas erbt von lv_img, dessen Konstruktor LV_OBJ_FLAG_CLICKABLE
    // aktiv entfernt (lv_img_constructor in lv_img.c) – ohne dieses Add hier reagiert
    // die Canvas NIE auf Touch-Events (genau der Bug, der zuvor "kein Ball antippbar" verursachte).
    lv_obj_add_flag(bbCanvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bbCanvas, cbBubbleTap, LV_EVENT_CLICKED, nullptr);
  } else {
    Serial.println("[Bubblebreaker] ps_malloc Canvas-Puffer fehlgeschlagen!");
  }

  bbGameOverPanel = lv_obj_create(bbScreen);
  lv_obj_set_size(bbGameOverPanel, 300, 140);
  lv_obj_align(bbGameOverPanel, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(bbGameOverPanel, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(bbGameOverPanel, 220, 0);
  lv_obj_set_style_radius(bbGameOverPanel, 10, 0);
  lv_obj_clear_flag(bbGameOverPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(bbGameOverPanel, LV_OBJ_FLAG_HIDDEN);

  bbLblGameOverTitel = lv_label_create(bbGameOverPanel);
  lv_label_set_text(bbLblGameOverTitel, "Spiel zu Ende");
  lv_obj_set_style_text_font(bbLblGameOverTitel, &font_montserrat_20, 0);
  lv_obj_set_style_text_color(bbLblGameOverTitel, lv_color_white(), 0);
  lv_obj_align(bbLblGameOverTitel, LV_ALIGN_TOP_MID, 0, 12);

  bbLblGameOverScore = lv_label_create(bbGameOverPanel);
  lv_label_set_text(bbLblGameOverScore, "Punkte: 0");
  lv_obj_set_style_text_font(bbLblGameOverScore, &font_montserrat_16, 0);
  lv_obj_set_style_text_color(bbLblGameOverScore, lv_color_white(), 0);
  lv_obj_set_style_text_align(bbLblGameOverScore, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(bbLblGameOverScore, LV_ALIGN_TOP_MID, 0, 44);

  lv_obj_t* nochmalBtn = erstelleTextButton(bbGameOverPanel, 20, 85, 120, 40, 0x2ecc71, "Nochmal");
  lv_obj_add_event_cb(nochmalBtn, cbBBNochmalTap, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* menuBtn = erstelleTextButton(bbGameOverPanel, 160, 85, 120, 40, 0x7f8c8d, "Menue");
  lv_obj_add_event_cb(menuBtn, cbBBZurueckMenuTap, LV_EVENT_CLICKED, nullptr);
}

// ============================================================
//  Taschenrechner (programmatisch; wird beim Oeffnen angelegt und beim Schliessen wieder freigegeben)
// ============================================================
static lv_obj_t* rcScreen = nullptr;
static lv_obj_t* rcWert   = nullptr;   // grosse Anzeige
static lv_obj_t* rcFormel = nullptr;   // kleine Zeile darueber
static String    rcEingabe = "0";      // aktuelle Eingabe, '.' als Dezimaltrenner (Anzeige mit ',')
static String    rcFormelText = "";
static double    rcAkku = 0;
static char      rcOp = 0;             // '+', '-', '*', '/' oder 0
static bool      rcNeu = true;         // naechste Ziffer beginnt eine neue Eingabe
static bool      rcFehler = false;

// Gemeinsame Styles: ein Satz fuer alle Tasten statt Einzel-Styles pro Objekt (spart LVGL-Speicher)
static lv_style_t rcStBasis, rcStGedrueckt;
static lv_style_t rcStFarbe[7];
static bool rcStylesBereit = false;
enum { RC_ZIFFER, RC_OP, RC_CLEAR, RC_BACK, RC_PROZENT, RC_VZ, RC_GLEICH };
static const uint32_t RC_FARBEN[7] = { 0x34495e, 0x3498db, 0xe74c3c, 0xe67e22, 0x9b59b6, 0x16a085, 0x2ecc71 };

static String rcFormat(double v) {
  if (isnan(v) || isinf(v)) return "Fehler";
  char b[32];
  snprintf(b, sizeof(b), "%.10g", v);
  return String(b);
}
static String rcAnzeige(String s) { s.replace('.', ','); return s; }
static const char* rcOpText(char op) {
  switch (op) { case '+': return "+"; case '-': return "-"; case '*': return "x"; case '/': return "/"; }
  return "";
}

static void rcAktualisiere() {
  if (!rcWert || !rcFormel) return;
  lv_label_set_text(rcWert, rcFehler ? "Fehler" : rcAnzeige(rcEingabe).c_str());
  lv_label_set_text(rcFormel, rcFormelText.c_str());
}

static void rcAlles() {
  rcEingabe = "0"; rcFormelText = ""; rcAkku = 0; rcOp = 0; rcNeu = true; rcFehler = false;
}

static void rcFehlerSetzen(const String& formel) {
  rcFehler = true; rcOp = 0; rcNeu = true; rcFormelText = formel;
  rcAktualisiere();
}

static bool rcRechne(double a, char op, double b, double& out) {
  switch (op) {
    case '+': out = a + b; return true;
    case '-': out = a - b; return true;
    case '*': out = a * b; return true;
    case '/': if (b == 0) return false; out = a / b; return true;
  }
  out = b; return true;
}

static void rcZiffer(char c) {
  if (rcFehler) rcAlles();
  if (rcNeu) { rcEingabe = ""; rcNeu = false; }
  if (rcEingabe == "0") rcEingabe = "";
  if (rcEingabe.length() >= 12) return;
  rcEingabe += c;
  rcAktualisiere();
}

static void rcKomma() {
  if (rcFehler) rcAlles();
  if (rcNeu) { rcEingabe = "0"; rcNeu = false; }
  if (rcEingabe.indexOf('.') < 0 && rcEingabe.length() < 11) rcEingabe += ".";
  rcAktualisiere();
}

static void rcOperator(char op) {
  if (rcFehler) return;
  double wert = rcEingabe.toDouble();
  if (!rcOp) {
    rcAkku = wert;                                  // erster Operator
  } else if (!rcNeu) {                              // Kettenrechnung: 2 + 3 + ... rechnet zuerst 2 + 3
    double erg;
    String formel = rcAnzeige(rcFormat(rcAkku)) + " " + rcOpText(rcOp) + " " + rcAnzeige(rcEingabe);
    if (!rcRechne(rcAkku, rcOp, wert, erg)) { rcFehlerSetzen(formel + " ="); return; }
    rcAkku = erg;
  }                                                 // (Operator direkt nach Operator: nur wechseln)
  rcOp = op;
  rcNeu = true;
  rcEingabe = rcFormat(rcAkku);
  rcFormelText = rcAnzeige(rcEingabe) + " " + rcOpText(op);
  rcAktualisiere();
}

static void rcGleich() {
  if (rcFehler || !rcOp) return;
  double b = rcEingabe.toDouble(), erg;
  String formel = rcAnzeige(rcFormat(rcAkku)) + " " + rcOpText(rcOp) + " " + rcAnzeige(rcEingabe) + " =";
  if (!rcRechne(rcAkku, rcOp, b, erg)) { rcFehlerSetzen(formel); return; }
  rcAkku = erg;
  rcEingabe = rcFormat(erg);
  rcFormelText = formel;
  rcOp = 0;
  rcNeu = true;
  rcAktualisiere();
}

static void rcProzent() {
  if (rcFehler) return;
  double w = rcEingabe.toDouble();
  w = (rcOp == '+' || rcOp == '-') ? rcAkku * w / 100.0 : w / 100.0;   // 200 + 10 % = 200 + 20
  rcEingabe = rcFormat(w);
  rcNeu = false;
  rcAktualisiere();
}

static void rcVorzeichen() {
  if (rcFehler || rcEingabe == "0") return;
  if (rcEingabe.startsWith("-")) rcEingabe.remove(0, 1); else rcEingabe = "-" + rcEingabe;
  rcAktualisiere();
}

static void rcRueckschritt() {
  if (rcFehler) { rcAlles(); rcAktualisiere(); return; }
  if (rcNeu) return;
  rcEingabe.remove(rcEingabe.length() - 1);
  if (rcEingabe.length() == 0 || rcEingabe == "-") rcEingabe = "0";
  rcAktualisiere();
}

static void rcSchliessen() {
  loadScreen(SCREEN_ID_SCREENMENU);                 // Rechner ist danach nicht mehr aktiv
  if (rcScreen) lv_obj_del_async(rcScreen);         // erst nach dem Callback loeschen (Taste ist Kind des Screens)
  rcScreen = rcWert = rcFormel = nullptr;
}

static void cbRechnerTaste(lv_event_t* e) {
  char c = (char)(intptr_t)lv_event_get_user_data(e);
  switch (c) {
    case 'C': rcAlles(); rcAktualisiere(); break;
    case 'B': rcRueckschritt(); break;
    case 'P': rcProzent(); break;
    case 'N': rcVorzeichen(); break;
    case '.': rcKomma(); break;
    case '=': rcGleich(); break;
    case '+': case '-': case '*': case '/': rcOperator(c); break;
    case 'X': rcSchliessen(); break;
    default:  if (c >= '0' && c <= '9') rcZiffer(c); break;
  }
}

static void rcStylesAnlegen() {
  if (rcStylesBereit) return;
  lv_style_init(&rcStBasis);
  lv_style_set_radius(&rcStBasis, 10);
  lv_style_set_border_width(&rcStBasis, 0);
  lv_style_set_shadow_width(&rcStBasis, 0);
  lv_style_set_outline_width(&rcStBasis, 0);
  lv_style_set_bg_opa(&rcStBasis, LV_OPA_COVER);
  lv_style_set_text_color(&rcStBasis, lv_color_white());
  lv_style_set_text_font(&rcStBasis, &lv_font_montserrat_28);
  lv_style_init(&rcStGedrueckt);
  lv_style_set_bg_opa(&rcStGedrueckt, LV_OPA_60);
  for (int i = 0; i < 7; i++) {
    lv_style_init(&rcStFarbe[i]);
    lv_style_set_bg_color(&rcStFarbe[i], lv_color_hex(RC_FARBEN[i]));
  }
  rcStylesBereit = true;
}

static void erstelleRechnerScreen() {
  rcStylesAnlegen();
  rcScreen = lv_obj_create(nullptr);
  lv_obj_set_size(rcScreen, 480, 320);
  lv_obj_set_style_bg_color(rcScreen, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(rcScreen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(rcScreen, LV_OBJ_FLAG_SCROLLABLE);

  // Anzeige
  lv_obj_t* panel = lv_obj_create(rcScreen);
  lv_obj_set_pos(panel, 6, 4);
  lv_obj_set_size(panel, 468, 62);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x1b2631), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(panel, 10, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  rcFormel = lv_label_create(panel);
  lv_label_set_long_mode(rcFormel, LV_LABEL_LONG_DOT);
  lv_obj_set_width(rcFormel, 400);
  lv_obj_set_pos(rcFormel, 10, 5);
  lv_obj_set_style_text_font(rcFormel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(rcFormel, lv_color_hex(0x85c1e9), 0);

  rcWert = lv_label_create(panel);
  lv_obj_set_width(rcWert, 448);
  lv_obj_set_style_text_align(rcWert, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(rcWert, 10, 20);
  lv_obj_set_style_text_font(rcWert, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(rcWert, lv_color_white(), 0);

  // Schliessen oben rechts in der Anzeige (wie beim Bubblebreaker)
  lv_obj_t* ex = lv_btn_create(panel);
  lv_obj_set_pos(ex, 428, 4);
  lv_obj_set_size(ex, 34, 26);
  lv_obj_set_style_bg_color(ex, lv_color_hex(0xcc0000), 0);
  lv_obj_set_style_radius(ex, 6, 0);
  lv_obj_set_style_shadow_width(ex, 0, 0);
  lv_obj_t* exl = lv_label_create(ex);
  lv_label_set_text(exl, LV_SYMBOL_CLOSE);
  lv_obj_set_style_text_color(exl, lv_color_white(), 0);
  lv_obj_center(exl);
  lv_obj_add_event_cb(ex, cbRechnerTaste, LV_EVENT_CLICKED, (void*)(intptr_t)'X');

  // Tastenfeld 4 x 5
  struct Taste { char code; const char* text; int art; };
  static const Taste TASTEN[5][4] = {
    { {'C', "C", RC_CLEAR}, {'B', LV_SYMBOL_LEFT, RC_BACK}, {'P', "%", RC_PROZENT}, {'/', "/", RC_OP} },
    { {'7', "7", RC_ZIFFER}, {'8', "8", RC_ZIFFER}, {'9', "9", RC_ZIFFER}, {'*', "x", RC_OP} },
    { {'4', "4", RC_ZIFFER}, {'5', "5", RC_ZIFFER}, {'6', "6", RC_ZIFFER}, {'-', "-", RC_OP} },
    { {'1', "1", RC_ZIFFER}, {'2', "2", RC_ZIFFER}, {'3', "3", RC_ZIFFER}, {'+', "+", RC_OP} },
    { {'N', "+/-", RC_VZ}, {'0', "0", RC_ZIFFER}, {'.', ",", RC_ZIFFER}, {'=', "=", RC_GLEICH} },
  };
  for (int r = 0; r < 5; r++) {
    for (int k = 0; k < 4; k++) {
      const Taste& t = TASTEN[r][k];
      lv_obj_t* b = lv_btn_create(rcScreen);
      lv_obj_set_pos(b, 6 + k * 118, 72 + r * 50);
      lv_obj_set_size(b, 114, 46);
      lv_obj_add_style(b, &rcStBasis, 0);
      lv_obj_add_style(b, &rcStFarbe[t.art], 0);
      lv_obj_add_style(b, &rcStGedrueckt, LV_STATE_PRESSED);
      lv_obj_t* l = lv_label_create(b);
      lv_label_set_text(l, t.text);
      lv_obj_center(l);
      lv_obj_add_event_cb(b, cbRechnerTaste, LV_EVENT_CLICKED, (void*)(intptr_t)t.code);
    }
  }
}

static void cbOpenRechner(lv_event_t*) {
  size_t vorher = lvglFreiBytes();
  if (vorher < 24000) {   // sonst droht ein LVGL-Out-of-Memory beim Anlegen
    logDiag("[Rechner] zu wenig Speicher: frei %u", (unsigned)vorher);
    return;
  }
  if (rcScreen) { lv_obj_del(rcScreen); rcScreen = rcWert = rcFormel = nullptr; }   // Rest eines nie geschlossenen Rechners
  rcAlles();
  erstelleRechnerScreen();
  rcAktualisiere();
  lv_scr_load(rcScreen);
  size_t nachher = lvglFreiBytes();
  logDiag("[Rechner] geoeffnet, Speicher frei: %u (vorher %u), Bedarf %d Bytes", (unsigned)nachher, (unsigned)vorher, (int)((long)vorher - (long)nachher));
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

static void cbMenuBioBoxSwitch(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  cfg.main_box = lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0;
  Preferences prefs; prefs.begin("wcp", false);
  prefs.putInt("main_box", cfg.main_box);
  prefs.end();
  aktualisiereUI();
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
  // DWD-Warn-Button auf screen_1 sofort anpassen; beim Einschalten holt die Hauptschleife die Warnungen gleich
  if (!cfg.warn_region) {
    anzahl_warnungen = 0; dwdWarnBestaetigt = false;
    if (dwdWarnBtn) lv_obj_add_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
    if (dwdBlinker) { lv_timer_del(dwdBlinker); dwdBlinker = nullptr; }
  } else {
    letztesWarnUpdate = millis() - 900000UL;
  }
}

static void cbMenu(lv_event_t*) {
  loadScreen(SCREEN_ID_SCREENMENU);
  // Switch-Zustände und Arc auf aktuelle cfg-Werte setzen
  if (objects.bioboxswitch) {
    if (cfg.main_box == 1) lv_obj_add_state(objects.bioboxswitch, LV_STATE_CHECKED);
    else                   lv_obj_clear_state(objects.bioboxswitch, LV_STATE_CHECKED);
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
static void cbFwd2(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENVERLAUF); }               // forecastwetter >
static void cbBack2(lv_event_t*) { loadScreen(SCREEN_ID_SCREENVERLAUF); }               // forecastpollen <
static void cbVlBack(lv_event_t*) { loadScreen(SCREEN_ID_SCREENFORECASTWETTER); }       // verlauf <
static void cbVlFwd(lv_event_t*)  { loadScreen(SCREEN_ID_SCREENFORECASTPOLLEN); }       // verlauf >
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
static void cbBack8(lv_event_t*)      { loadScreen(SCREEN_ID_SCREENBIOWETTER); }       // screenhealth < → biowetter
static void cbFwd8(lv_event_t*)       { loadScreen(SCREEN_ID_SCREENFORECASTWETTER); }  // screenhealth > → forecastwetter
static void cbHealthHome(lv_event_t*) { loadScreen(SCREEN_ID_SCREEN_1); }              // screenhealth fc_home → screen_1
static void cbHealthMenu(lv_event_t*) { loadScreen(SCREEN_ID_SCREENMENU); }            // screenhealth fc_settings2 → screenmenu
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
  if (cfg.warn_region && anzahl_warnungen > 0) {
    lv_obj_clear_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
    if (!dwdWarnBestaetigt && !dwdBlinker)
      dwdBlinker = lv_timer_create(cbDwdBlinker, 600, nullptr);
  } else {
    lv_obj_add_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
    dwdWarnBestaetigt = false;
    if (dwdBlinker) { lv_timer_del(dwdBlinker); dwdBlinker = nullptr; }
  }
}

// ── Update-Hinweis: kleines Badge oben rechts auf Screen 1 + Popup ──
static void cbUpdatePopupSchliessen(lv_event_t*) {
  if (updatePopup) { lv_obj_del(updatePopup); updatePopup = nullptr; }
}

static void cbUpdatePopupInstallieren(lv_event_t*) {
  if (updatePopup) { lv_obj_del(updatePopup); updatePopup = nullptr; }
  otaStartAngefordert = true;
}

static void zeigeUpdatePopup() {
  if (updatePopup || !objects.screen_1) return;

  // Vollflaechiges, abgedunkeltes Overlay: faengt Touches ab, damit nichts darunter ausgeloest wird
  updatePopup = lv_obj_create(objects.screen_1);
  lv_obj_set_size(updatePopup, 480, 320);
  lv_obj_set_pos(updatePopup, 0, 0);
  lv_obj_set_style_bg_color(updatePopup, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(updatePopup, LV_OPA_60, 0);
  lv_obj_set_style_border_width(updatePopup, 0, 0);
  lv_obj_set_style_radius(updatePopup, 0, 0);
  lv_obj_clear_flag(updatePopup, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* karte = lv_obj_create(updatePopup);
  lv_obj_set_size(karte, 420, 240);
  lv_obj_align(karte, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(karte, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(karte, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(karte, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_width(karte, 2, 0);
  lv_obj_set_style_radius(karte, 10, 0);
  lv_obj_clear_flag(karte, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* titel = lv_label_create(karte);
  lv_label_set_text(titel, LV_SYMBOL_DOWNLOAD "  Update verfuegbar");
  lv_obj_set_style_text_font(titel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(titel, lv_color_hex(0xf5a623), 0);
  lv_obj_align(titel, LV_ALIGN_TOP_MID, 0, 6);

  String text = "Neue Version: " + updateNeueVersion + "\nInstalliert: " + String(FIRMWARE_VERSION) +
                "\n\nDas Update dauert ca. 1 Minute. Das Geraet dabei nicht vom Strom trennen.";
  lv_obj_t* lbl = lv_label_create(karte);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl, 380);
  lv_label_set_text(lbl, text.c_str());
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xeeeeee), 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 44);

  lv_obj_t* btnJa = lv_btn_create(karte);
  lv_obj_set_size(btnJa, 190, 42);
  lv_obj_align(btnJa, LV_ALIGN_BOTTOM_LEFT, 8, -6);
  lv_obj_set_style_bg_color(btnJa, lv_color_hex(0x2e8b57), 0);
  lv_obj_t* lblJa = lv_label_create(btnJa);
  lv_label_set_text(lblJa, "Jetzt installieren");
  lv_obj_set_style_text_color(lblJa, lv_color_hex(0xffffff), 0);
  lv_obj_center(lblJa);
  lv_obj_add_event_cb(btnJa, cbUpdatePopupInstallieren, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* btnNein = lv_btn_create(karte);
  lv_obj_set_size(btnNein, 150, 42);
  lv_obj_align(btnNein, LV_ALIGN_BOTTOM_RIGHT, -8, -6);
  lv_obj_set_style_bg_color(btnNein, lv_color_hex(0x333333), 0);
  lv_obj_set_style_border_color(btnNein, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_width(btnNein, 1, 0);
  lv_obj_t* lblNein = lv_label_create(btnNein);
  lv_label_set_text(lblNein, "Spaeter");
  lv_obj_set_style_text_color(lblNein, lv_color_hex(0xffffff), 0);
  lv_obj_center(lblNein);
  lv_obj_add_event_cb(btnNein, cbUpdatePopupSchliessen, LV_EVENT_CLICKED, nullptr);
}

static void cbUpdateBadgeTap(lv_event_t*) { zeigeUpdatePopup(); }

void erstelleUpdateBadge() {
  if (updateBadge || !objects.screen_1) return;
  // Oben rechts: dort ist auf Screen 1 frei (Datum endet bei x=328, die Info-Boxen beginnen bei y=44)
  updateBadge = lv_btn_create(objects.screen_1);
  lv_obj_set_size(updateBadge, 92, 32);
  lv_obj_set_pos(updateBadge, 384, 6);
  lv_obj_set_style_bg_color(updateBadge, lv_color_hex(0xf5a623), 0);
  lv_obj_set_style_bg_opa(updateBadge, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(updateBadge, 16, 0);
  lv_obj_set_style_border_width(updateBadge, 0, 0);
  lv_obj_set_style_shadow_width(updateBadge, 0, 0);
  lv_obj_t* lbl = lv_label_create(updateBadge);
  lv_label_set_text(lbl, LV_SYMBOL_DOWNLOAD " Update");
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl);
  lv_obj_add_flag(updateBadge, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(updateBadge, cbUpdateBadgeTap, LV_EVENT_CLICKED, nullptr);
}

void aktualisiereUpdateBadge() {
  if (!updateBadge) return;
  if (updateVerfuegbar) lv_obj_clear_flag(updateBadge, LV_OBJ_FLAG_HIDDEN);
  else                  lv_obj_add_flag(updateBadge, LV_OBJ_FLAG_HIDDEN);
}

// ── Blinken auf Warnscreens (500 ms, Icon + Titel) ──────────
static bool warnBlinkState = false;
static void cbWarnBlink(lv_timer_t*) {
  warnBlinkState = !warnBlinkState;
  lv_obj_t* aktiv = lv_scr_act();
  if (aktiv == objects.screenwarnungpollen) {
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
  pollenWarnBestaetigt = true;
  loadScreen(SCREEN_ID_SCREEN_1);
}

static void zeigeWarnPopup(lv_obj_t* parent, const char* titel, const char* text, bool nurSchliessen) {
  if (warnPopup) { lv_obj_del(warnPopup); warnPopup = nullptr; }
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

  if (src == objects.screenwarnungpollen) {
    String pollenName = ohneUmlaute(schleimsterPollen());
    float maxP = -1; for (int i=0;i<8;i++) if (pollenGewaehlt(i)) maxP=max(maxP,pollenWertWarnung(i));
    const char* stufe = maxP>=3?"stark":(maxP>=2?"hoch":(maxP>=1?"mittel":"gering"));
    String msg = pollenName + ": " + stufe + " Belastung\n(Stufe " + String((int)maxP) + " von 3)";
    msg += "\nSchwelle: >" + String(cfg.pollen_schwelle);
    zeigeWarnPopup(objects.screenwarnungpollen, "Pollenwarnung", msg.c_str(), false);
  }
}

// ============================================================
//  SETUP
// ============================================================
#if !VARIANTE_EUROPE
// Namensnennung der Datenquellen: die Labels (labelquelle*) kommen aus PicoPixel (Text, Lage und Farbe dort); hier wird nur die Schrift gesetzt.
static void quellenHinweiseAnlegen() {
  lv_obj_t* l[] = {objects.labelquellestart, objects.labelquellehaupt, objects.labelquellegesundheit, objects.labelquellebio2,
                   objects.labelquellekarte1, objects.labelquellekarte2, objects.labelquellemenue};
  for (lv_obj_t* o : l) if (o) lv_obj_set_style_text_font(o, &font_montserrat_14, 0);
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("\n[WetterCubePlus] Start v" FIRMWARE_VERSION);
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);   // frueh setzen, damit Log-Zeitstempel lokal sind (configTime() setzt TZ spaeter zurueck, siehe unten)
  tzset();
  {
    esp_reset_reason_t rr = esp_reset_reason();
    bool absturz = (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT);
    if (absturz && spurMagic == 0x5350554Cu && spurText[0]) { spurText[sizeof(spurText) - 1] = 0; spurVorAbsturz = spurText; }
    spurMagic = 0; spurText[0] = 0;
  }
  resetGrundText = resetGrundName(esp_reset_reason());
  logDiag("[Boot] v%s, Neustart-Grund: %s", FIRMWARE_VERSION, resetGrundText.c_str());
  if (spurVorAbsturz.length()) logDiag("[Boot] Zuletzt aktiv vor dem Neustart: %s", spurVorAbsturz.c_str());

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
#if !VARIANTE_EUROPE
  quellenHinweiseAnlegen();
#endif
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

  // Hinweis-Kachel (Regen/Frost/UV) auf Screen 1: antippbar, blinkt bis zur Bestaetigung
  if (objects.labelregen) {
    lv_obj_add_flag(objects.labelregen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(objects.labelregen, 8);
    lv_obj_set_style_text_font(objects.labelregen, &font_montserrat_14, LV_PART_MAIN);   // enthaelt ä ö ü ß °, die eingebaute Standardschrift nicht
    lv_obj_add_event_cb(objects.labelregen, cbHinweisTap, LV_EVENT_CLICKED, nullptr);
  }
  lv_timer_create(cbHinweisBlink, 500, nullptr);

  // Warnkarte-Screen programmatisch anlegen
  erstelleWarnkarteScreen();
  erstelleWarnkarte2Screen();

  // Bubblebreaker-Minispiel programmatisch anlegen
  erstelleBreakerSelectScreen();
  erstelleBubbleBreakerScreen();
  bbLadeHighscores();

  // Navigations-Buttons sofort verdrahten (vor WiFi-Check, gilt auch im Portal-Modus)
  // Hauptkette: screen_1 → forecastwetter → forecastpollen → screenwarnkarte1 → screensunmoon → screenairquality → screen_1
  // Seitenpfad: screen_1 ← (back) → screenbiowetter ↔ screenbiowetter2
  #define REG_CB(obj, cb, evt) do { if (obj) lv_obj_add_event_cb(obj, cb, evt, nullptr); } while(0)
  // Hauptkette Pfeile
  REG_CB(objects.labelbuttonforward,    cbFwd1,  LV_EVENT_CLICKED);  // screen_1 >
  REG_CB(objects.labelbuttonbackward_2, cbBack1, LV_EVENT_CLICKED);  // forecastwetter <
  REG_CB(objects.labelbuttonforward_2,  cbFwd2,  LV_EVENT_CLICKED);  // forecastwetter >
  REG_CB(objects.labelbuttonbackward_1, cbBack2, LV_EVENT_CLICKED);  // forecastpollen <
  REG_CB(objects.labelbuttonbackwardverlauf, cbVlBack, LV_EVENT_CLICKED);  // verlauf <
  REG_CB(objects.labelbuttonforwardverlauf,  cbVlFwd,  LV_EVENT_CLICKED);  // verlauf >
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
  if (objects.fc_settings)        lv_obj_add_flag(objects.fc_settings,        LV_OBJ_FLAG_CLICKABLE);
  if (objects.labelbuttonforward) lv_obj_add_flag(objects.labelbuttonforward, LV_OBJ_FLAG_CLICKABLE);
  if (objects.labelbuttonbackward)lv_obj_add_flag(objects.labelbuttonbackward,LV_OBJ_FLAG_CLICKABLE);
  REG_CB(objects.fc_settings,   cbMenu, LV_EVENT_CLICKED);  // screen_1 → screenmenu
  REG_CB(objects.labelbuttonmenu_2, cbHome, LV_EVENT_CLICKED);  // screenmenu → screen_1
  REG_CB(objects.buttonbreaker, cbOpenBreakerSelect, LV_EVENT_CLICKED);  // screenmenu → Bubblebreaker-Auswahl
  REG_CB(objects.buttoncalc,    cbOpenRechner,       LV_EVENT_CLICKED);  // screenmenu → Taschenrechner
  REG_CB(objects.button_4, cbHome, LV_EVENT_CLICKED);  // screenforecastpollenhour → screen_1
  // Nach Theme-Wechsel Farbkodierungen wiederherstellen
  if (objects.labelswitchtheme)
    lv_obj_add_event_cb(objects.labelswitchtheme, [](lv_event_t*) { aktualisiereUI(); }, LV_EVENT_CLICKED, nullptr);
  // Menü-Screen Controls
  REG_CB(objects.labellightchangevalue, cbMenuBrightness,   LV_EVENT_VALUE_CHANGED);
  REG_CB(objects.pollenswitch,          cbMenuPollenSwitch, LV_EVENT_VALUE_CHANGED);
  REG_CB(objects.bioboxswitch,          cbMenuBioBoxSwitch, LV_EVENT_VALUE_CHANGED);
  REG_CB(objects.dwdswitch,             cbMenuDwdSwitch,    LV_EVENT_VALUE_CHANGED);
  // Home-Buttons
  REG_CB(objects.labelbuttonhome,   cbHome, LV_EVENT_CLICKED);  // forecastpollen
  REG_CB(objects.labelbuttonhome_1, cbHome, LV_EVENT_CLICKED);  // forecastwetter
  REG_CB(objects.labelbuttonhomeverlauf, cbHome, LV_EVENT_CLICKED);  // verlauf
  REG_CB(objects.labelbuttonhome_2, cbHome, LV_EVENT_CLICKED);  // screenwarnkarte1
  REG_CB(objects.labelbuttonhome_3, cbHome, LV_EVENT_CLICKED);  // screenwarnkarte2
  REG_CB(objects.labelbuttonhome_4, cbHome, LV_EVENT_CLICKED);  // screensunmoon
  REG_CB(objects.labelbuttonhome_5, cbHome, LV_EVENT_CLICKED);  // screenairquality
  REG_CB(objects.labelbuttonhome_7, cbHome, LV_EVENT_CLICKED);  // screenbiowetter
  REG_CB(objects.labelbuttonhome_8, cbHome, LV_EVENT_CLICKED);  // screenbiowetter2
  // screenhealth navigation
  REG_CB(objects.labelbuttonbackward_6, cbBack8,      LV_EVENT_CLICKED);  // < → biowetter
  REG_CB(objects.labelbuttonforward_7,  cbFwd8,       LV_EVENT_CLICKED);  // > → forecastwetter
  if (objects.fc_home)           lv_obj_add_flag(objects.fc_home,           LV_OBJ_FLAG_CLICKABLE);
  if (objects.fc_settings2)      lv_obj_add_flag(objects.fc_settings2,      LV_OBJ_FLAG_CLICKABLE);
  if (objects.fc_plus_38_ffffff) lv_obj_add_flag(objects.fc_plus_38_ffffff, LV_OBJ_FLAG_CLICKABLE);
  REG_CB(objects.fc_home,           cbHealthHome,                LV_EVENT_CLICKED);  // fc_home → screen_1
  REG_CB(objects.fc_settings2,      cbHealthMenu,                LV_EVENT_CLICKED);  // fc_settings2 → screenmenu
  REG_CB(objects.fc_plus_38_ffffff, [](lv_event_t*) { loadScreen(SCREEN_ID_SCREENHEALTH); }, LV_EVENT_CLICKED);  // screen_1 → screenhealth
  // Warn-Screens
  REG_CB(objects.screenwarnungpollen, cbWarnTap,       LV_EVENT_CLICKED);
  REG_CB(objects.screenwarnkarte2,    cbWarnkarte2Tap, LV_EVENT_CLICKED);
  #undef REG_CB

  // Screenhealth Footer-Labels als farbige Pill-Badges initialisieren
  auto initDot = [](lv_obj_t* obj) {
    if (!obj) return;
    lv_obj_set_size(obj, 20, 20);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x555555), 0);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_label_set_text(obj, "");
  };
  initDot(objects.labelwsaqi);
  initDot(objects.labelwspm25);
  initDot(objects.labelwso3);
  initDot(objects.labelwsuv);

  // Warnliste-Screen ist clickbar für Popup (registriert via REG_CB auf screenwarnkarte2)

  // WiFi verbinden oder Portal starten
  if (cfg.ssid.isEmpty()) {
    portalPasswortNeu();
    { char t[240]; snprintf(t, sizeof(t), "Bitte den WetterCube konfigurieren.\n\nVerbinde dein Geraet mit:\nWLAN: WetterCubePlus-Setup\nPasswort: %s\n\nEs oeffnet sich ein Konfigurationsportal.\nFalls nicht: im Browser\nhttp://192.168.4.1 aufrufen.", portalPasswort); zeigeHinweisScreen(t); }
    startePortal();
    return;
  }

  zeigeBootScreen("Verbinde mit " + cfg.ssid + "...");
  setzeBootFortschritt(10);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(wifiEventCb, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  // Zwei Versuche (je 15 s): ein neuer WiFi.begin() hilft, wenn der Router beim ersten Mal noch nicht bereit war
  uint32_t t0 = millis();
  for (int versuch = 1; versuch <= 2 && WiFi.status() != WL_CONNECTED; versuch++) {
    wifiLetzterGrund = 0;
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
    uint32_t tv = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tv < 15000) {
      lvgl_flush(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
      logDiag("[WLAN] Versuch %d fehlgeschlagen: %s", versuch, wifiGrundName(wifiLetzterGrund));
      if (versuch < 2) { WiFi.disconnect(); delay(300); }
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiStartInfo = "verbunden nach " + String((millis() - t0) / 1000.0, 1) + " s, Signal " + String(WiFi.RSSI()) + " dBm";
    logDiag("[WLAN] %s", wifiStartInfo.c_str());
  } else {
    wifiStartInfo = String("FEHLGESCHLAGEN: ") + wifiGrundName(wifiLetzterGrund);
    logDiag("[WLAN] Verbindung beim Start fehlgeschlagen, Einrichtungsmodus (Wiederholung alle 60 s)");
  }

  if (WiFi.status() != WL_CONNECTED) {
    portalPasswortNeu();
    { char t[260]; snprintf(t, sizeof(t), "WLAN-Verbindung fehlgeschlagen.\n\nVerbinde dich mit:\nWLAN: WetterCubePlus-Setup\nPasswort: %s\n\nEs oeffnet sich ein Konfigurationsportal.\nFalls nicht: im Browser\nhttp://192.168.4.1 aufrufen.\n\nSSID und Passwort pruefen.", portalPasswort); zeigeHinweisScreen(t); }
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
    logDiag("[Start] mDNS: %s.local", MDNS_NAME);
  } else {
    logDiag("[Start] mDNS konnte nicht gestartet werden (Zugriff nur per IP-Adresse)");
  }

  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  zeigeBootScreen("Synchronisiere Zeit...");
  setzeBootFortschritt(45);
  { struct tm ti; int r = 0; while (!getLocalTime(&ti) && r++ < 30) { delay(300); lv_timer_handler(); } }

  starteWebUI();
  logDiag("[Start] Web-UI laeuft: http://%s/", WiFi.localIP().toString().c_str());
  setzeBootFortschritt(55);

  if (cfg.lat == 0.0f && !cfg.location.isEmpty()) {
    zeigeBootScreen("Koordinaten für " + cfg.location + "...");
    if (geocodeLocation(cfg.location)) speichereCfg();
    setzeBootFortschritt(65);
  }

  zeigeBootScreen("Lade Wetterdaten...");
  setzeBootFortschritt(70);
  bool ersterWetterFetchOk = fetchWetter();
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
  erstelleUpdateBadge();
  // Kurz warten damit 100% sichtbar ist, dann Hauptscreen laden
  delay(400);
  // LVGL-Animation-Timing: 600ms lv_timer_handler() vor weiteren Calls (Lessons Learned!)
  { unsigned long e = millis() + 600; while (millis() < e) { lv_timer_handler(); delay(5); } }
  loadScreen(SCREEN_ID_SCREEN_1);

  logDiag("[Boot] LVGL-Speicher: %s", lvglSpeicherText().c_str());
  logDiag("[Boot] Heap frei %u, PSRAM frei %u von %u", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(), (unsigned)ESP.getPsramSize());

  letztesTouchZeit    = millis();
  // Beim Fehlschlag des ersten Abrufs nach 60 s statt 10 Min. erneut versuchen
  letztesDatenUpdate  = millis() - (ersterWetterFetchOk ? 0UL : 540000UL);
  letztesPollenUpdate = millis();
  letztesWarnUpdate   = millis();
  letzteUhrzeit       = millis();

  Serial.println("[Setup] Bereit.");
}

// ============================================================
//  LOOP
// ============================================================
static const char* aktiverScreenName() {
  lv_obj_t* a = lv_scr_act();
  struct { lv_obj_t* o; const char* n; } t[] = {
    {objects.screen_1, "Hauptscreen"}, {objects.screenforecastwetter, "Vorhersage"}, {objects.screenverlauf, "Temperaturverlauf"},
    {objects.screenforecastpollen, "Pollen (Tage)"}, {objects.screenforecastpollenhour, "Pollen (Stunden)"},
    {objects.screenwarnkarte1, "Radarkarte"}, {objects.screenwarnkarte2, "DWD-Warnliste"}, {objects.screensunmoon, "Sonne und Mond"},
    {objects.screenairquality, "Luftqualitaet"}, {objects.screenbiowetter, "Biowetter 1"}, {objects.screenbiowetter2, "Biowetter 2"},
    {objects.screenhealth, "Gesundheit"}, {objects.screenmenu, "Menue"}, {objects.screenwarnungpollen, "Pollenwarnung"},
  };
  for (auto& e : t) if (e.o && e.o == a) return e.n;
  return nullptr;
}

void loop() {
  lv_timer_handler();
  ui_tick();
  {
    static lv_obj_t* letzterScreen = nullptr;
    lv_obj_t* a = lv_scr_act();
    if (a != letzterScreen) {
      letzterScreen = a;
      const char* n = aktiverScreenName();
      char buf[48]; snprintf(buf, sizeof(buf), "Screen: %s", n ? n : "anderer");
      spur(buf);
    }
  }

  // Portal-Modus: DNS + WebServer bedienen
  if (portal_modus) {
    dnsServer.processNextRequest();
    server.handleClient();
    portalWlanRetry();
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
      if (objects.labeltime)   lv_label_set_text(objects.labeltime,   buf);
      if (objects.labelwstime) lv_label_set_text(objects.labelwstime, buf);
      aktualisiereHinweise();
      const char* wt[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
      char dbuf[24];
      snprintf(dbuf, sizeof(dbuf), "%s. %02d.%02d.%04d",
        wt[ti.tm_wday], ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
      if (objects.labeldatum)   lv_label_set_text(objects.labeldatum,   dbuf);
      if (objects.labeldatum_1) lv_label_set_text(objects.labeldatum_1, dbuf);
    }
  }

  // Wetterdaten alle 10 Minuten
  if (millis() - letztesDatenUpdate >= 600000UL) {
    letztesDatenUpdate = millis();
    if (!fetchWetter()) letztesDatenUpdate -= 540000UL;   // Fehlschlag: nach 60 s erneut versuchen
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

  // Update-Pruefung: 2 Min. nach dem Start, danach alle 6 Stunden (nach einem Fehler nach 1 Stunde)
  if (millis() - letzterUpdateCheck >= updateCheckIntervall) {
    pruefeUpdate();
    aktualisiereUpdateBadge();
  }

  // Installation per Touch (Popup "Jetzt installieren"): hier statt im LVGL-Callback starten
  if (otaStartAngefordert) {
    otaStartAngefordert = false;
    String fehler = installiereUpdate();
    if (fehler.length() == 0) updateErfolgNeustart();
    else                      updateFehlerAnzeigen(fehler);
  }

  // Warnbildschirme einmalig anzeigen wenn neue Warnung
  if (pollenWarnAktiv && !pollenWarnBestaetigt && !pollenWarnGezeigt) {
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

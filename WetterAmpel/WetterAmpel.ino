// WetterAmpel — ESP32-C3 Super Mini
// 3x WS2812B (GPIO 8): Index 0 = grün (unten), 1 = gelb (mitte), 2 = rot (oben)
// Fragt WetterCubePlus /api/ampel alle 30s ab und setzt die passende LED.
// Bei aktiver DWD-Warnung blinken alle 3 LEDs rot bis die Warnung im Cube quittiert ist.

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>

// ── Konfiguration ────────────────────────────────────────────────────────────
#define WIFI_SSID       "xxx"
#define WIFI_PASSWORD   "xxx"
#define CUBE_HOST       "192.168.x.x"   // IP des WetterCubePlus im LAN
#define CUBE_PORT       80
#define POLL_INTERVAL   30000           // ms zwischen zwei Abfragen

#define LED_PIN         8
#define NUM_LEDS        3
#define LED_BRIGHTNESS  80              // 0–255

#define DWD_BLINK_ON    400             // ms LED an beim DWD-Blinken
#define DWD_BLINK_OFF   300             // ms LED aus beim DWD-Blinken

// LED-Indizes
#define IDX_GREEN   0
#define IDX_YELLOW  1
#define IDX_RED     2
// ─────────────────────────────────────────────────────────────────────────────

CRGB leds[NUM_LEDS];

unsigned long lastPoll    = 0;
bool          dwdWarning  = false;   // aktive DWD-Warnung?
String        ampelActive = "";      // zuletzt empfangener Temperaturstatus

// Blink-State für DWD (non-blocking)
bool          blinkState  = false;
unsigned long lastBlink   = 0;

// ── Hilfsfunktionen ──────────────────────────────────────────────────────────

void allOff() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void setAmpel(const String& active) {
  allOff();
  if (active == "green")       leds[IDX_GREEN]  = CRGB::Green;
  else if (active == "yellow") leds[IDX_YELLOW] = CRGB(255, 180, 0);
  else if (active == "red")    leds[IDX_RED]    = CRGB::Red;
  FastLED.show();
}

// Fehler-Blinken: alle LEDs kurz weiß (blockierend, nur kurz)
void blinkError() {
  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
  delay(200);
  allOff();
}

// Wird im Loop aufgerufen — non-blocking DWD-Rot-Blinken
void handleDwdBlink() {
  unsigned long now = millis();
  unsigned long interval = blinkState ? DWD_BLINK_ON : DWD_BLINK_OFF;

  if (now - lastBlink >= interval) {
    lastBlink = now;
    blinkState = !blinkState;

    if (blinkState) {
      fill_solid(leds, NUM_LEDS, CRGB::Red);
    } else {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
    FastLED.show();
  }
}

void fetchAndApply() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String("http://") + CUBE_HOST + ":" + CUBE_PORT + "/api/ampel";
  http.begin(url);
  http.setTimeout(5000);

  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (!err) {
      bool newDwd = doc["dwd_warning"] | false;
      const char* active = doc["active"] | "";

      ampelActive = String(active);
      bool dwdCleared = dwdWarning && !newDwd;
      dwdWarning = newDwd;

      if (dwdWarning) {
        // DWD aktiv → Blink-State initialisieren falls neu
        if (dwdCleared == false && blinkState == false) {
          lastBlink  = millis();
          blinkState = false;
        }
        Serial.println("[Ampel] DWD-Warnung aktiv → rot blinken");
      } else {
        // Keine Warnung → normale Ampelfarbe zeigen
        if (dwdCleared) Serial.println("[Ampel] DWD-Warnung quittiert → normale Anzeige");
        blinkState = false;
        setAmpel(ampelActive);
      }

      Serial.printf("[Ampel] active=%s  dwd=%s  temp=%.1f\n",
                    active,
                    dwdWarning ? "JA" : "nein",
                    doc["temperature"].as<float>());
    } else {
      Serial.printf("[Ampel] JSON-Fehler: %s\n", err.c_str());
      blinkError();
    }
  } else {
    Serial.printf("[Ampel] HTTP-Fehler: %d\n", code);
    blinkError();
  }
  http.end();
}

// ── Setup & Loop ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  allOff();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Verbinde");

  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Verbunden: %s\n", WiFi.localIP().toString().c_str());
    fetchAndApply();
    lastPoll = millis();
  } else {
    Serial.println("\n[WiFi] Verbindung fehlgeschlagen — retry im Loop");
    blinkError();
  }
}

void loop() {
  // WiFi-Reconnect falls verloren
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }

  // DWD-Blinken (non-blocking, läuft dauerhaft wenn aktiv)
  if (dwdWarning) {
    handleDwdBlink();
  }

  // Periodische Abfrage
  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();
    fetchAndApply();
  }
}

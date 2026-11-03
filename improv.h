// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// WLAN per USB-Kabel einrichten (Improv Serial, https://www.improv-wifi.com/serial/): WetterCube Control oder ein Web-Installer schickt WLAN-Name und
// Passwort ueber die serielle Schnittstelle (UART0, "USB-UART"-Anschluss) oder den eingebauten USB-Anschluss (USB-Serial/JTAG). Das Geraet nimmt sie nur
// im Einrichtungsmodus (Portal) an, probiert die Verbindung aus und speichert sie erst bei Erfolg. Das Passwort wird nie ins Log geschrieben.
// Wird in WetterCubePlus.ino (und WetterCubePlus-Europe.ino) nach den Definitionen von cfg, portal_modus und logDiag eingebunden.
// Pakete: "IMPROV", Version 1, Typ, Laenge, Daten, Pruefsumme (Summe aller Bytes davor), Zeilenumbruch. Geschrieben wird roh (kein println).
#pragma once
#include "hal/usb_serial_jtag_ll.h"
#ifndef TX
#define TX(de, en) de
#endif

namespace improv {
enum { T_ZUSTAND = 1, T_FEHLER = 2, T_RPC = 3, T_ERGEBNIS = 4 };
enum { C_WLAN = 1, C_ZUSTAND = 2, C_INFO = 3, C_NETZE = 4 };
enum { Z_GESTOPPT = 0, Z_BEREIT = 2, Z_VERBINDET = 3, Z_VERBUNDEN = 4 };
enum { F_KEINER = 0, F_PAKET = 1, F_BEFEHL = 2, F_VERBINDUNG = 3, F_UNBEKANNT = 255 };

static int quelle = 0;                       // woher die letzte Anfrage kam: 0 = UART0 (Serial), 1 = USB-Serial/JTAG; die Antwort geht dorthin zurueck
static volatile bool anfrage = false;
static char ssid[33], pass[64];

static void rohSchreiben(const uint8_t* b, size_t k) {
  if (quelle == 1) {
    size_t i = 0; unsigned long t0 = millis();
    while (i < k && millis() - t0 < 300) {
      int n = usb_serial_jtag_ll_write_txfifo(b + i, k - i);
      i += n;
      usb_serial_jtag_ll_txfifo_flush();
      if (n == 0) delay(1);
    }
    return;
  }
  Serial.write(b, k);
  Serial.flush();
}

static void senden(uint8_t typ, const uint8_t* d, size_t n) {
  uint8_t b[272];
  if (n > 255) return;
  memcpy(b, "IMPROV", 6);
  b[6] = 1; b[7] = typ; b[8] = (uint8_t)n;
  if (n) memcpy(b + 9, d, n);
  size_t k = 9 + n;
  uint8_t s = 0;
  for (size_t i = 0; i < k; i++) s += b[i];
  b[k++] = s; b[k++] = '\n';
  rohSchreiben(b, k);
}
static void zustand(uint8_t z) { senden(T_ZUSTAND, &z, 1); }
static void fehler(uint8_t f) { senden(T_FEHLER, &f, 1); }

static size_t text(uint8_t* ziel, size_t pos, const char* t) {      // Zeichenkette mit vorangestellter Laenge anhaengen
  size_t n = strlen(t); if (n > 100) n = 100;
  ziel[pos++] = (uint8_t)n; memcpy(ziel + pos, t, n);
  return pos + n;
}
static void ergebnis(uint8_t befehl, const uint8_t* texte, size_t n) {   // Befehlsnummer, Laenge, dann die Zeichenketten
  uint8_t d[260]; d[0] = befehl; d[1] = (uint8_t)n; if (n) memcpy(d + 2, texte, n);
  senden(T_ERGEBNIS, d, n + 2);
}

static void adresseMelden() {
  uint8_t t[120]; char url[80];
  snprintf(url, sizeof(url), "http://%s/", WiFi.localIP().toString().c_str());
  ergebnis(C_WLAN, t, text(t, 0, url));
}
static void zustandMelden() {
  if (WiFi.status() == WL_CONNECTED && !portal_modus) { zustand(Z_VERBUNDEN); adresseMelden(); }
  else if (portal_modus) zustand(Z_BEREIT);
  else zustand(Z_GESTOPPT);                      // Start laeuft noch
}

static void befehl(const uint8_t* d, size_t laenge) {   // d = Befehlsnummer, Datenlaenge, Daten
  if (laenge < 2 || (size_t)d[1] + 2 > laenge) { fehler(F_PAKET); return; }
  const uint8_t cmd = d[0], dl = d[1];
  const uint8_t* nutz = d + 2;
  fehler(F_KEINER);
  if (cmd == C_ZUSTAND) { zustandMelden(); return; }
  if (cmd == C_INFO) {
    uint8_t t[200]; size_t k = 0;
    k = text(t, k, IMPROV_GERAET); k = text(t, k, FIRMWARE_VERSION); k = text(t, k, "esp32s3"); k = text(t, k, MDNS_NAME);
    ergebnis(C_INFO, t, k);
    return;
  }
  if (cmd == C_NETZE) {
    if (portal_modus) {
      const int n = WiFi.scanNetworks();
      for (int i = 0; i < n && i < 20; i++) {
        String s = WiFi.SSID(i);
        if (s.isEmpty()) continue;
        uint8_t t[140]; char r[12]; snprintf(r, sizeof(r), "%d", (int)WiFi.RSSI(i));
        size_t k = text(t, 0, s.c_str()); k = text(t, k, r); k = text(t, k, WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "YES" : "NO");
        ergebnis(C_NETZE, t, k);
      }
      WiFi.scanDelete();
    }
    ergebnis(C_NETZE, nullptr, 0);                       // leeres Ergebnis: Liste zu Ende
    return;
  }
  if (cmd == C_WLAN) {
    if (!portal_modus) { fehler(F_UNBEKANNT); logDiag(TX("[Improv] WLAN-Daten abgelehnt: Geraet ist nicht im Einrichtungsmodus", "[Improv] Wi-Fi data rejected: device is not in setup mode")); return; }
    if (dl < 2) { fehler(F_PAKET); return; }
    const size_t sl = nutz[0];
    if (sl == 0 || sl > 32 || 1 + sl + 1 > dl) { fehler(F_PAKET); return; }
    const size_t pl = nutz[1 + sl];
    if (pl > 63 || 1 + sl + 1 + pl > dl) { fehler(F_PAKET); return; }
    if (anfrage) { fehler(F_UNBEKANNT); return; }        // der letzte Versuch laeuft noch
    memcpy(ssid, nutz + 1, sl); ssid[sl] = 0;
    memcpy(pass, nutz + 2 + sl, pl); pass[pl] = 0;
    anfrage = true;
    zustand(Z_VERBINDET);
    logDiag(TX("[Improv] WLAN-Daten per Kabel erhalten (Netz '%s')", "[Improv] Wi-Fi data received over the cable (network '%s')"), ssid);
    return;
  }
  fehler(F_BEFEHL);
}

struct Leser { uint8_t puf[272]; size_t n = 0; };
static Leser leser[2];
static void fuettern(int q, uint8_t b) {
  static const char KOPF[] = "IMPROV";
  Leser& l = leser[q];
  if (l.n < 6) {
    if (b == (uint8_t)KOPF[l.n]) { l.puf[l.n++] = b; return; }
    l.n = 0;
    if (b == (uint8_t)KOPF[0]) l.puf[l.n++] = b;
    return;
  }
  l.puf[l.n++] = b;
  if (l.n < 9) return;
  const size_t gesamt = 9 + (size_t)l.puf[8] + 1;
  if (gesamt > sizeof(l.puf)) { l.n = 0; return; }
  if (l.n < gesamt) return;
  uint8_t s = 0;
  for (size_t i = 0; i + 1 < gesamt; i++) s += l.puf[i];
  const bool ok = l.puf[6] == 1 && s == l.puf[gesamt - 1];
  quelle = q;
  if (!ok) fehler(F_PAKET);
  else if (l.puf[7] == T_RPC) befehl(l.puf + 9, l.puf[8]);
  l.n = 0;
}
}  // namespace improv

// Aus loop() aufrufen: liest eingehende Pakete (UART0 und USB-Serial/JTAG)
static void improvTick() {
  int budget = 256;
  while (budget-- > 0 && Serial.available()) improv::fuettern(0, (uint8_t)Serial.read());
  uint8_t puf[64];
  while (usb_serial_jtag_ll_rxfifo_data_available()) {
    const int m = usb_serial_jtag_ll_read_rxfifo(puf, sizeof(puf));
    for (int i = 0; i < m; i++) improv::fuettern(1, puf[i]);
  }
}

// Im Einrichtungsmodus aus loop() aufrufen: fuehrt einen angeforderten Verbindungsversuch aus
static void improvPortalTick() {
  using namespace improv;
  if (!anfrage) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, pass);
  const unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) { delay(50); }
  if (WiFi.status() == WL_CONNECTED) {
    cfg.ssid = ssid; cfg.pass = pass;                    // Ort und Region bleiben, wie sie sind
    speichereCfg();
    logDiag(TX("[Improv] Verbunden mit '%s', gespeichert, Neustart", "[Improv] Connected to '%s', saved, restarting"), ssid);
    zustand(Z_VERBUNDEN);
    adresseMelden();
    memset(pass, 0, sizeof(pass)); anfrage = false;
    delay(1500);
    ESP.restart();
  } else {
    logDiag(TX("[Improv] Verbindung mit '%s' fehlgeschlagen", "[Improv] Connection to '%s' failed"), ssid);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);                                   // zurueck in den reinen Einrichtungsmodus (Portal-WLAN bleibt)
    memset(pass, 0, sizeof(pass)); anfrage = false;
    fehler(F_VERBINDUNG);
    zustand(Z_BEREIT);
  }
}

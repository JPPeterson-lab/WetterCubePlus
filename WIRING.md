# WetterCubePlus – Verkabelung

## ESP32-S3 N16R8 ↔ ILI9488 3,5" + XPT2046 Touch

### Display (ILI9488) – SPI2

| Display-Pin | ESP32-S3 GPIO | Hinweis |
|---|---|---|
| VCC | 3,3 V | Nicht 5V! |
| GND | GND | |
| CS | GPIO 15 | Chip Select Display |
| RST | GPIO 16 | Reset |
| DC / RS | GPIO 2 | Data/Command |
| MOSI / SDI | GPIO 13 | SPI Data |
| SCK / CLK | GPIO 14 | SPI Clock |
| LED / BL | GPIO 17 | Backlight (PWM) |
| MISO / SDO | GPIO 12 | (optional, nur für Lesebefehle) |

### Touch (XPT2046) – EIGENER SPI-Bus (SPI3)

Das Modul "3.5'' TFT SPI 480x320 V1.0" hat separate Touch-Pins –
T_CLK/T_DIN/T_DO sind NICHT mit SCK/SDI/SDO verbunden!

| Touch-Pin | ESP32-S3 GPIO | Hinweis |
|---|---|---|
| T_CLK | **GPIO 6** | eigener SPI3-Bus |
| T_DIN | **GPIO 7** | Touch MOSI |
| T_DO  | **GPIO 8** | Touch MISO |
| T_CS  | GPIO 21 | Chip Select |
| T_IRQ | GPIO 18 | nicht genutzt (Polling-Modus) |

### Spannungsversorgung

| ESP32-S3 Pin | Anschluss |
|---|---|
| 5V / VIN | USB oder extern 5V |
| 3,3 V | Display VCC, Touch VCC |
| GND | gemeinsame Masse |

### Hinweise

- **GPIO 33–37** sind beim N16R8 intern für das OPI-PSRAM belegt → nicht verwenden!
- **GPIO 19/20** sind USB D−/D+ → für normale I/O vermeiden
- **GPIO 0** ist Strapping-Pin (Boot) → für Inputs nur mit Pull-Up
- Das ILI9488-Modul hat meist einen integrierten 3,3V-Pegelwandler auf der Platine.
  Wenn das Display-Modul mit 5V arbeitet, trotzdem nur 3,3V an VCC anlegen, sofern kein
  separater 3,3V-Eingang vorhanden.
- Die Hintergrundbeleuchtung (BL/LED) kann direkt an GPIO 17 angeschlossen werden.
  Für hellere LEDs einen NPN-Transistor (z.B. BC547) als Schalter verwenden.

### Schematische Übersicht

```
ESP32-S3 N16R8
┌─────────────────────┐
│  GPIO 2  ────────── DC (Display)
│  GPIO 12 ────────── MISO (Display + Touch)
│  GPIO 13 ────────── MOSI (Display + Touch)
│  GPIO 14 ────────── SCLK (Display + Touch)
│  GPIO 15 ────────── CS   (Display)
│  GPIO 16 ────────── RST  (Display)
│  GPIO 17 ────────── BL   (Backlight)
│  GPIO 18 ────────── IRQ  (Touch)
│  GPIO 21 ────────── CS   (Touch)
│  3.3V    ────────── VCC  (Display + Touch)
│  GND     ────────── GND  (alle)
└─────────────────────┘
```

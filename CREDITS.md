# Credits & Drittanbieter-Lizenzen

Dieses Projekt verwendet Ressourcen Dritter. Die jeweiligen Lizenzbedingungen
sind nachfolgend aufgeführt.

---

## Wetter-Icons

**Quelle:** [Meteocons](https://github.com/basmilius/weather-icons) von Bas Milius

**Lizenz:** MIT License (Copyright (c) 2020-present Bas Milius)

Verwendete Icons (Stil „Fill“, statische Variante, als PNG mit 128×128 Pixeln in PicoPixel eingebunden und als Arrays für LVGL eingebettet; Dateien unter `src/ui/images/`):
- `day_clear`, `day_partial_cloud`, `day_rain`, `day_snow` – Tag: klar, teilweise bewölkt, Regenschauer, Schneeschauer
- `night_full_moon_clear`, `night_full_moon_partial_cloud`, `night_full_moon_rain`, `night_full_moon_snow` – Nacht (Mond) mit denselben Wetterlagen
- `overcast` – bedeckt, `fog` – Nebel, `mist` – Dunst
- `rain` – Regen und Nieselregen, `rain_thunder` – Regen mit Gewitter, `thunder` – Gewitter
- `sleet` – Schneeregen, `snow` – Schnee
- `alert` – Warn-Icon (Pollenwarnung)

Änderung gegenüber dem Original: Die Umrandung der Wolken und der Nebelstreifen wurde auf ein mittleres Blaugrau (`#94A3B8`) gesetzt, damit sie auch auf dem hellen Farbthema sichtbar sind.

Lizenztext der Meteocons (MIT):

```
MIT License

Copyright (c) 2020-present Bas Milius

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Bis einschließlich Version 1.6.4 nutzte das Projekt stattdessen Wetter-Icons von
[Dovora](https://www.dovora.com/resources/weather-icons/) (CC BY-SA 4.0). Diese sind
seit Version 1.6.5 nicht mehr enthalten; in älteren Versionen (Git-Verlauf, frühere
Releases) stecken sie noch und bleiben dort unter CC BY-SA 4.0 (Urheber: Dovora).

---

## Bedienelemente-Icons

**Quelle:** [Icons8 Flat Color Icons](https://github.com/icons8/flat-color-icons) – © Icons8

**Lizenz:** Die Icons stehen unter der MIT-Lizenz **oder** der [Good Boy License](https://icons8.com/good-boy-license/) von Icons8 (laut [LICENSE.md](https://github.com/icons8/flat-color-icons/blob/master/LICENSE.md)). Erlaubt sind Herunterladen, Ändern und Weitergeben, auch kommerziell; eine Namensnennung ist nicht vorgeschrieben, wird hier aber gern gegeben.

Verwendete Icons (als PNG in PicoPixel eingebunden, als Arrays für LVGL eingebettet):
- `home` – Home-Taste (`fc_home`)
- `settings` – Menü-Zahnrad (`fc_settings`)
- `left` und `right` – Vor- und Zurück-Pfeile (`fc_left_60`, `fc_right_60`)
- `left_down2` und `right_up2` – diagonale Pfeile (`fc_left_down2_38`, `fc_right_up2_38`)
- `plus` – Plus-Taste (`fc_plus_38_ffffff`, weiß eingefärbt)
- `calculator` – Taschenrechner (`fc_calculator_38`)
- `self_service_kiosk` – Spiel Bubblebreaker (`fc_self_service_kiosk_38`)

---

## Wetterdaten

**Quelle:** [Open-Meteo](https://open-meteo.com) – Wetter, Pollen, Luftqualität, Standortsuche

**Lizenz der Daten:** [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (Namensnennung). Die Nutzung der kostenlosen API ist für nicht-kommerzielle
Zwecke vorgesehen, ein API-Key ist nicht erforderlich.

---

## DWD-Daten

**Quelle:** [Deutscher Wetterdienst (DWD) – Opendata](https://opendata.dwd.de)

Wetterwarnungen, Pollenflug-Vorhersagen und Warnkarte werden vom DWD kostenfrei
bereitgestellt. Nutzung gemäß den [DWD-Nutzungsbedingungen](https://www.dwd.de/DE/service/copyright/copyright_artikel.html).

---

## Schrift

**Quelle:** [Montserrat](https://github.com/JulietaUla/Montserrat) – © The Montserrat Project Authors

**Lizenz:** [SIL Open Font License 1.1](https://openfontlicense.org)

Die Montserrat-Schrift ist in mehreren Größen (14 bis 52 Pixel) als umgewandelte C-Dateien
(`src/ui/fonts/`) im Projekt enthalten. Die OFL erlaubt das Einbetten in Software; die Schrift selbst
darf nicht einzeln verkauft werden.

---

## Bibliotheken und Framework

Diese Bibliotheken werden beim Selbstbauen über den Arduino-Bibliotheksverwalter installiert und stecken
kompiliert in den veröffentlichten Firmware-Dateien. Ihre Quellen liegen nicht in diesem Repository.

| Bibliothek | Lizenz |
|---|---|
| [LVGL](https://lvgl.io) (Light and Versatile Graphics Library), enthält [LodePNG](https://lodev.org/lodepng/) | MIT; LodePNG: zlib |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) (Display- und Touch-Treiber) | MIT und BSD-2-Clause |
| [ArduinoJson](https://arduinojson.org) | MIT |
| [FastLED](https://fastled.io) (nur WetterAmpel) | MIT |
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32) (Framework) | LGPL 2.1 |
| [ESP-IDF](https://github.com/espressif/esp-idf) (darunter) | Apache 2.0 |

Die Quellen dieses Projekts sind vollständig öffentlich; die Firmware lässt sich damit jederzeit selbst bauen.

---

## Projektlizenz

Dieses Projekt steht unter der **Creative Commons Attribution-NonCommercial 4.0
International (CC BY-NC 4.0)** – siehe [LICENSE](LICENSE).

Die oben genannten Drittanbieter-Ressourcen unterliegen ihren jeweiligen Lizenzen
(Meteocons: MIT, Icons8: MIT oder Good Boy License, Montserrat: SIL OFL 1.1, Bibliotheken siehe oben)
und sind von der CC BY-NC 4.0 dieses Projekts nicht erfasst.

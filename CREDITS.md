# Credits & third-party licences / Credits & Drittanbieter-Lizenzen

> **English summary.** This project uses third-party resources. The sections below are in German; licence texts are quoted in their original language. In short:
>
> - **Weather data / Wetterdaten:** "Weather data by [Open-Meteo.com](https://open-meteo.com)", licence [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) ([open-meteo.com/en/licence](https://open-meteo.com/en/licence)). Values are partly converted and summarised in the WetterCube (for example pollen levels and daily maxima). The free API is for **non-commercial use only**; commercial use needs a licence from Open-Meteo.
> - **Location search:** Location data by [GeoNames](https://www.geonames.org) via Open-Meteo (CC BY 4.0).
> - **Pollen and air quality:** "Contains modified Copernicus Atmosphere Monitoring Service information", provided via Open-Meteo. Neither the European Commission nor ECMWF is responsible for any use of the data.
> - **German Weather Service (DWD):** Warnings, pollen forecast, bio-weather and warning map from [DWD Opendata](https://opendata.dwd.de), licence CC BY 4.0, credited as "Quelle: Deutscher Wetterdienst" next to the displayed data.
> - **Weather icons:** [Meteocons](https://github.com/basmilius/weather-icons) by Bas Milius (MIT); outlines recoloured for the light theme. Versions up to 1.6.4 used [Dovora](https://www.dovora.com/resources/weather-icons/) icons (CC BY-SA 4.0), which remain licensed that way in older releases.
> - **UI icons:** [Icons8 Flat Color Icons](https://github.com/icons8/flat-color-icons) (MIT or Good Boy License).
> - **Font:** [Montserrat](https://github.com/JulietaUla/Montserrat) (SIL OFL 1.1).
> - **Libraries:** LVGL incl. LodePNG (MIT / zlib), LovyanGFX (MIT / BSD-2-Clause), ArduinoJson (MIT), FastLED (MIT, WetterAmpel only), Arduino-ESP32 (LGPL 2.1), ESP-IDF (Apache 2.0).
> - **UI design and export:** [PicoPixel](https://picopixel.io)
>
> **Project licence:** source code Apache License 2.0 ([LICENSE](LICENSE), [NOTICE](NOTICE)); texts, images, documentation and 3D print files CC BY-NC 4.0 ([LICENSE-Dokumentation.txt](LICENSE-Dokumentation.txt)). Third-party resources keep their own licences.

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

„Weather data by [Open-Meteo.com](https://open-meteo.com)“. Lizenz und Quellen der Daten: [open-meteo.com/en/licence](https://open-meteo.com/en/licence). Die Werte werden im WetterCube umgerechnet und zusammengefasst (zum Beispiel Pollenstufen und Tageshöchstwerte). Orte werden über die Geocoding-Schnittstelle gesucht, deren Daten von [GeoNames](https://www.geonames.org) stammen (CC BY 4.0).

**Lizenz der Daten:** [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (Namensnennung, Open-Meteo.com). Die kostenlose API ist nur für **nicht-kommerzielle** Nutzung vorgesehen ([Nutzungsbedingungen](https://open-meteo.com/en/terms)), ein API-Key ist nicht erforderlich. Die Vorhersagen beruhen auf Modellen nationaler Wetterdienste wie DWD, NOAA, ECMWF und Météo-France (Einzelheiten: [open-meteo.com/en/docs](https://open-meteo.com/en/docs)).

**Pollen und Luftqualität:** Daten des **Copernicus Atmosphere Monitoring Service (CAMS)** des ECMWF, bereitgestellt über Open-Meteo. „Contains modified Copernicus Atmosphere Monitoring Service information.“ Weder die Europäische Kommission noch das ECMWF haften für die Verwendung der Daten.

---

## DWD-Daten

**Quelle:** [Deutscher Wetterdienst (DWD) – Opendata](https://opendata.dwd.de)

Wetterwarnungen, Pollenflug-Vorhersagen, Biowetter und Warnkarte stammen vom DWD (frei zugängliche Geodaten). Sie dürfen nach den
[Rechtlichen Hinweisen des DWD](https://www.dwd.de/DE/service/rechtliche_hinweise/rechtliche_hinweise_node.html) unter der Lizenz
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) mit Quellenvermerk weiterverwendet werden. Der Vermerk lautet
**„Quelle: Deutscher Wetterdienst“** und steht unmittelbar an den angezeigten DWD-Informationen; bei weitergehend bearbeiteten Daten
nennt der DWD Hinweise wie „Datenbasis: Deutscher Wetterdienst, …“ in zentralen Quellenverzeichnissen.

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

Der Quellcode dieses Projekts steht unter der **Apache License 2.0** – siehe [LICENSE](LICENSE) und [NOTICE](NOTICE). Texte, Bilder, Dokumentation und die 3D-Druckdateien stehen unter **CC BY-NC 4.0** ([LICENSE-Dokumentation.txt](LICENSE-Dokumentation.txt)).

Die oben genannten Drittanbieter-Ressourcen unterliegen ihren jeweiligen Lizenzen (Meteocons: MIT, Icons8: MIT oder Good Boy License, Montserrat: SIL OFL 1.1, Bibliotheken siehe oben) und sind von der Lizenz dieses Projekts nicht erfasst.

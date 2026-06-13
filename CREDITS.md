# Credits & Drittanbieter-Lizenzen

Dieses Projekt verwendet Ressourcen Dritter. Die jeweiligen Lizenzbedingungen
sind nachfolgend aufgeführt.

---

## Wetter-Icons

**Quelle:** [Dovora Weather Icons](https://www.dovora.com/resources/weather-icons/)

**Lizenz:** [Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)](https://creativecommons.org/licenses/by-sa/4.0/)

Verwendete Icons (256×256, RGB565A8):
- `day_clear.inc` – Klarer Himmel (WMO 0–3); auch Fallback für Bewölkung und Nebel (WMO 4–48)
- `rain.inc` – Regen, Nieselregen, Schauer (WMO 51–82); auch Fallback für Gewitter (WMO 95–99)
- `snow.inc` – Schnee und Schneeschauer (WMO 71–86)
- `alert.inc` – Warnscreen-Icon

> **Hinweis:** Icons für Bewölkung (overcast), Nebel (fog) und Gewitter (thunder) sind noch nicht eingebunden – WMO-Codes ohne dediziertes Icon fallen auf `day_clear` bzw. `rain` zurück.

Gemäß CC BY-SA 4.0 gilt:
- Die Icons wurden als RGB565A8-Arrays für LVGL 8.x konvertiert und eingebettet
- Der Urheber (Dovora) wird hiermit ausdrücklich genannt
- Weitergabe dieser Icons muss ebenfalls unter CC BY-SA 4.0 erfolgen

---

## Wetterdaten

**Quelle:** [Open-Meteo](https://open-meteo.com)

Open-Meteo ist eine kostenlose Wetter-API für nicht-kommerzielle Nutzung.
Kein API-Key erforderlich.

---

## DWD-Daten

**Quelle:** [Deutscher Wetterdienst (DWD) – Opendata](https://opendata.dwd.de)

Wetterwarnungen, Pollenflug-Vorhersagen und Warnkarte werden vom DWD kostenfrei
bereitgestellt. Nutzung gemäß den [DWD-Nutzungsbedingungen](https://www.dwd.de/DE/service/copyright/copyright_artikel.html).

---

## UI-Framework

**Quelle:** [LVGL](https://lvgl.io) – Light and Versatile Graphics Library

**Lizenz:** MIT License

---

## Projektlizenz

Der eigentliche Quellcode (`.ino`, `.c`, `.h` Dateien) dieses Projekts steht
unter der **MIT-Lizenz** – siehe [LICENSE](LICENSE).

Die oben genannten Drittanbieter-Ressourcen unterliegen ihren jeweiligen Lizenzen.
Die Verwendung der Icons in diesem Projekt macht den Quellcode nicht zu einem
abgeleiteten Werk der Icons; die MIT-Lizenz bleibt für den Code hiervon unberührt.

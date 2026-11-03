# Changelog

*[English version](CHANGELOG.en.md)*

## v2.1.16 (2026-11-05)

### Neu
- **Lesende Status-Seite für andere Programme:** Das Gerät liefert unter `/api/status` die aktuellen Werte (Temperatur, Wetterlage, Luftqualität, Zahl der DWD-Warnungen) als JSON. Sie braucht kein Passwort und kann nichts verändern, genau wie die bestehende Ampel-Schnittstelle `/api/ampel`; wie jede Seite des Geräts ist sie nur im Heimnetz und nur über die eigene Adresse des Geräts erreichbar. Gedacht für Begleitprogramme und Smart-Home-Werkzeuge im eigenen Netz.

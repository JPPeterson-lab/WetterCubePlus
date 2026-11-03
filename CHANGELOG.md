# Changelog

*[English version](CHANGELOG.en.md)*

## v2.1.13 (2026-11-01)

### Behoben
- **DWD-Warnungen nur für deinen Ort:** Bisher fragte das Gerät alle Warnungen in einem Rechteck um das ganze Bundesland ab. Dadurch erschienen auch Warnungen aus Nachbarländern und fernen Landesteilen, obwohl für den eigenen Ort keine Warnung galt, und echte Warnungen für den Ort konnten durch die Begrenzung auf fünf Meldungen verdrängt werden. Jetzt werden nur Warnungen angezeigt, deren Gemeinde deinen Standort enthält. Ohne eingerichteten Standort gilt weiter der Ausschnitt des Bundeslandes.

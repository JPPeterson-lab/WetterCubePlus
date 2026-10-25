# Changelog

*[English version](CHANGELOG.en.md)*

## v2.1.12 (2026-10-25)

### Behoben
- **Biowetter und Pollenflug vor 11 Uhr:** Der DWD erneuert beide Dateien nur einmal täglich gegen 11 Uhr. Bis dahin steht in „heute“ noch das Datum von gestern; das Gerät zeigte deshalb am Vormittag um einen Tag verschobene Werte und „sprang“ nach dem Update. Jetzt zählt das Datum der einzelnen Zeiträume bzw. das Datum der Datei, gestrige Zeiträume fallen weg. Ist die Datei noch von gestern, holt das Gerät sie alle 20 Minuten erneut (auch direkt nach dem Start).
- Das Log nennt das Dateidatum („Datei vom … = VERALTET“).

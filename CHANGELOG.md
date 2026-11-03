# Changelog

*[English version](CHANGELOG.en.md)*

## v2.1.20 (2026-11-12)

### Behoben
- **Regenradar fiel zeitweise aus:** Der Kartenserver des Deutschen Wetterdienstes antwortet oft erst nach 8 bis 16 Sekunden und nimmt zeitweise keine Verbindung an; etwa jeder zweite Abruf scheiterte, die Karte blieb dann bis zu 20 Minuten alt. Der Cube gibt dem Verbindungsaufbau jetzt mehr Zeit, versucht es bei einem Verbindungsfehler sofort ein zweites Mal und nach einem Fehlschlag schon nach 2 statt nach 10 Minuten wieder. Den genauen Grund eines Fehlschlags schreibt er ins Protokoll.

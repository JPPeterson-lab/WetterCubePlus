# Changelog

*[English version](CHANGELOG.en.md)*

## v2.1.18 (2026-11-08)

### Neu
- **WLAN per USB-Kabel einrichten:** Im Einrichtungsmodus nimmt der Cube WLAN-Name und Passwort über das USB-Kabel an (Improv Serial, über den Anschluss UART und über den eingebauten USB-Anschluss). WetterCube Control und der Web-Installer können sie so ans Gerät schicken, du musst dich nicht mit dem Einrichtungs-WLAN verbinden. Das Gerät probiert die Verbindung aus und speichert sie erst bei Erfolg. Das Passwort wird nie ins Protokoll geschrieben.
- **Updates von der eigenen Website:** Der Cube fragt nach neuen Versionen bei `wettercube.de/dl/plus` und lädt die Firmware von dort. Antwortet die Website nicht, nimmt er wie bisher GitHub als Rückfall. Die Datenschutz-Hinweise in der Einrichtung und in den Einstellungen nennen jetzt `wettercube.de` (ersatzweise GitHub).

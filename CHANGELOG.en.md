# Changelog

*[Deutsche Version](CHANGELOG.md)*

## v2.1.18 (2026-11-08)

### New
- **Set up Wi-Fi over the USB cable:** in setup mode the cube accepts the Wi-Fi name and password over the USB cable (Improv Serial, through the UART port and through the built-in USB port). WetterCube Control and the web installer can send them to the device this way, you do not have to connect to the setup Wi-Fi. The device tries the connection and only saves it on success. The password is never written to the log.
- **Updates from our own website:** the cube asks for new versions at `wettercube.de/dl/plus` and downloads the firmware from there. If the website does not answer, it falls back to GitHub as before. The privacy notes in the setup and in the settings now name `wettercube.de` (GitHub as a fallback).

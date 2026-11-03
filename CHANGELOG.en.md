# Changelog

*[Deutsche Version](CHANGELOG.md)*

## v2.1.20 (2026-11-12)

### Fixed
- **Rain radar dropped out at times:** the map server of the German weather service (DWD) often takes 8 to 16 seconds to answer and at times refuses connections; about every second request failed and the map was then up to 20 minutes old. The cube now gives the connection more time, retries once right away after a connection error and, after a failure, tries again after 2 instead of 10 minutes. The exact reason of a failure is written to the log.

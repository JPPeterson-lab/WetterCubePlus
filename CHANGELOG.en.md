# Changelog

*[Deutsche Version](CHANGELOG.md)*

## v2.1.9 (2026-10-22)

### Fixed
- **Switching DWD warnings off:** The red warning button came back after the DWD warnings were switched off, although the switch was off (on the device and in the web interface). The device had kept the most recently fetched warnings and showed them again at the next refresh. Now they disappear immediately when switched off, and are fetched again right away when switched on.

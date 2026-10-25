# Changelog

*[Deutsche Version](CHANGELOG.md)*

## v2.1.12 (2026-10-25)

### Fixed
- **Biotropic forecast and pollen before 11 am:** The DWD renews both files only once a day, around 11 am. Until then "today" still carries yesterday's date, so the device showed values shifted by one day in the morning and "jumped" after the update. The date of each period (or of the file) now decides, yesterday's periods are dropped. If the file is still from yesterday, the device fetches it again every 20 minutes (also right after boot).
- The log states the file date ("file from … = OUTDATED").

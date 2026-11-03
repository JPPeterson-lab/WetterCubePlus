# Changelog

*[Deutsche Version](CHANGELOG.md)*

## v2.1.13 (2026-11-01)

### Fixed
- **DWD warnings only for your location:** The device used to request all warnings in a rectangle around the whole federal state, so warnings from neighbouring states and distant parts of the state could appear although none applied to your place, and real local warnings could be pushed out by the limit of five. Now only warnings whose municipality contains your location are shown. Without a configured location the state's bounding box is still used.

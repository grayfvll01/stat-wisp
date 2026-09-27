# Changelog

All notable changes to Stat Wisp are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.0.0] - 2026-09-27

First public release.

### Added

- Automatic updates: a daily check of the latest GitHub release, a tray notification, and one-click install. Downloads are verified against the published size and SHA-256 before use. The installed edition updates through its installer; the portable edition replaces its executable in place and restarts.
- **Check for updates** in the tray menu and **Check for updates automatically** in Settings. Administrators can turn updates off with the `DisableUpdates` policy value.
- A small diagnostic log (`stat-wisp.log`) and crash reports beside the settings file.
- The installer supports `/ALLUSERS` for machine-wide deployment, closes a running copy cleanly, and restarts it after an in-app update.
- About dialog with version and project link.

### Changed

- Redesigned Settings window: system font, sections, and correct scaling on high-DPI and mixed-DPI displays. It opens on the monitor in use and has a taskbar button.
- Single `VERSION` file drives the executable, installer, and release.
- Release notes come from this changelog; the installer no longer shows a license page.
- Windows shows the program as "Stat Wisp" in Task Manager and notifications.

### Fixed

- Network rates were counted several times on adapters with NDIS filter drivers.
- NVIDIA and AMD readings stayed unavailable when the display driver loaded after Stat Wisp; detection now retries.
- Fan speeds used 1,024-based thousands (3000 RPM showed as 2.9K).
- A failing sensor provider could stop monitoring; each provider is now isolated.
- Tray icons now appear even when Windows Explorer is not ready at sign-in.
- **Start with Windows** reflects the real startup entry after an uninstall or change by another tool.
- Settings files with a UTF-8 BOM or spaces around `=` are read correctly, saved data is flushed before replacement, and an unreadable file is kept as `settings.ini.bak`.

### Security

- DLLs load only from System32, and the process ends on heap corruption.
- Update downloads use HTTPS with certificate validation and are rejected unless their size and SHA-256 match the release manifest.

## [0.4.1] - 2026-09-12

Not published separately; included in 1.0.0.

### Fixed

- Windows GPU-usage fallback adds usage from processes sharing an engine before choosing the busiest engine.
- Network rates track each interface separately to avoid spikes when adapters change.
- AMD adapter identification and Overdrive 5 detection.
- Settings writes detect failures before replacing the saved file; invalid version numbers are rejected.
- Opening Settings blocks duplicate windows and conflicting tray changes.

## [0.4.0] - 2026-09-06

### Added

- Reliable bounded shutdown and accurate sensor fallback.

[Unreleased]: https://github.com/grayfvll01/stat-wisp/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/grayfvll01/stat-wisp/releases/tag/v1.0.0
[0.4.1]: https://github.com/grayfvll01/stat-wisp/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/grayfvll01/stat-wisp/releases/tag/v0.4.0

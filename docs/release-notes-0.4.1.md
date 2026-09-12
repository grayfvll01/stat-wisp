# Stat Wisp 0.4.1

Stat Wisp puts your selected PC readings in the Windows tray: temperatures, usage, memory, clocks, network and disk activity, battery, and supported fan readings.

## Changes in this release

- Windows GPU-usage fallback now adds usage from processes sharing a physical engine before selecting the busiest engine, correcting underreported totals.
- Network rates track each interface separately to avoid spikes or dropped readings when adapters connect, disconnect, or reset counters.
- Corrected AMD adapter identification and restricted legacy ADL calls to adapters reporting Overdrive 5. Newer AMD interfaces use the available Windows/WMI fallbacks.
- Settings writes detect failures before replacing the saved configuration, and first setup reports a failure to save.
- Invalid settings-version numbers are rejected instead of being treated as a legacy configuration.
- Opening Settings prevents duplicate settings windows and conflicting tray changes while the dialog is active.
- The Windows startup entry is updated when the startup preference changes, so changing a metric does not rewrite it.
- Release packaging checks versions, checksums, archive contents, and executable protections, with installer lifecycle checks in CI before a draft is prepared.
- Expanded installation, update, removal, and troubleshooting documentation, plus issue forms for reproducible reports.

## Download

For **Windows 10/11 x64**:

- **`stat-wisp-0.4.1-setup.exe`** — install for your Windows account, with Start menu and uninstall entries.
- **`stat-wisp-0.4.1-win-x64.zip`** — extract and run `stat-wisp.exe` without an installer.
- **`SHA256SUMS.txt`** — SHA-256 checksums for the attached assets.

Choose a Windows package above to run the app; GitHub's source archives are for building it yourself. Release binaries are unsigned, so Windows may display an unknown-publisher or reputation warning.

## Getting started

Select at least one metric in first setup and click **Start**. CPU Usage and RAM Usage work without an external hardware monitor. Find hidden icons in the tray overflow, hover for details, and right-click any icon for settings or **Exit Stat Wisp**. Starting with Windows is optional and off by default.

## Upgrading

Choose **Exit Stat Wisp**, then run the installer over the existing installation or replace the files in your portable folder. Preferences remain in `%LOCALAPPDATA%\stat-wisp\settings.ini`, shared by installed and portable editions. If you move the executable, toggle **Start with Windows** off and on to update its saved path.

## Sensor and platform limitations

- CPU temperature and CPU/system fan readings require a running compatible LibreHardwareMonitor or OpenHardwareMonitor WMI provider. Stat Wisp does not bundle sensor drivers.
- GPU metrics depend on vendor, device, and driver support. NVIDIA and AMD have different capabilities; GPU usage can fall back to Windows counters. Explicit GPU selection is not available.
- `--` means the selected reading is unavailable, not zero. Values can differ between monitoring tools because their sensor selection and aggregation differ.
- The app makes no outbound network requests and does not check for updates automatically.

## Validation

A clean Windows x64 Release build passed all five automated checks, including settings interaction and bounded shutdown. Package integrity, upgrade from the published v0.4.0 installer, repair, launch/Exit, and uninstall were also checked. See the [release review](https://github.com/grayfvll01/stat-wisp/blob/v0.4.1/docs/release-validation.md) for scope and hardware coverage limits.

[User guide](https://github.com/grayfvll01/stat-wisp/blob/v0.4.1/docs/user-guide.md) · [Sensor support](https://github.com/grayfvll01/stat-wisp/blob/v0.4.1/docs/sensor-support.md) · [Report a problem](https://github.com/grayfvll01/stat-wisp/issues/new/choose)

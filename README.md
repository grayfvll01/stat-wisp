# Stat Wisp

Your PC's vitals, quietly in the Windows tray.

Stat Wisp is a native Windows system monitor that shows the readings you choose as small icons beside the clock. Hover for details; right-click any icon for settings, pause, restart, or exit.

**[Download the latest release](https://github.com/grayfvll01/stat-wisp/releases/latest)** · [User guide](docs/user-guide.md) · [Sensor support](docs/sensor-support.md) · [Report a problem](https://github.com/grayfvll01/stat-wisp/issues/new/choose)

![Stat Wisp settings](docs/images/settings.png)

## What it shows

- CPU and GPU usage, temperature, and reported clocks.
- RAM usage, available memory, commit usage, and supported NVIDIA VRAM usage.
- Network transfer rates, disk transfer rates and activity, battery percentage, and supported fan readings.
- One icon per selected metric, temperature colors, Celsius or Fahrenheit, compact labels, and refresh intervals from 500 ms to one minute.

**Sensor availability depends on your hardware.** CPU temperature and CPU/system fan readings require a compatible running LibreHardwareMonitor or OpenHardwareMonitor WMI provider. NVIDIA and AMD GPU readings depend on the installed driver; `--` means unavailable. See the [support matrix](docs/sensor-support.md) before choosing sensors.

## Install

Requires **Windows 10 or 11, x64**. No administrator rights or separate runtime installation are required for Stat Wisp.

Open the [latest release](https://github.com/grayfvll01/stat-wisp/releases/latest), then choose an asset:

| Download | Use it for |
|---|---|
| `stat-wisp-<version>-setup.exe` | Installation for your Windows account, with Start menu and uninstall entries. |
| `stat-wisp-<version>-win-x64.zip` | Running without an installer: extract the ZIP and launch `stat-wisp.exe`. |
| `SHA256SUMS.txt` | [Checking your download](docs/user-guide.md#verify-a-download). |

The GitHub **Source code** archives are for developers; choose the setup executable or Windows ZIP to run the app. Current release packages are unsigned, so Windows may display an unknown-publisher or reputation warning.

On first launch, select at least one metric and click **Start**. If the icons are hidden, open the tray overflow beside the clock and move them into view. **Start with Windows** is optional and off by default.

## Update or remove

Choose **Exit Stat Wisp** before updating. Run the new installer over your installation, or replace the files in your extracted portable folder. Your settings are kept in `%LOCALAPPDATA%\stat-wisp\settings.ini`; the installed and portable editions share them. Updates are downloaded manually from GitHub Releases.

To remove an installed copy, use Windows' installed apps list or **Uninstall Stat Wisp** in the Start menu. To remove a portable copy, disable **Start with Windows**, exit the app, and delete its extracted folder. See the [user guide](docs/user-guide.md#uninstall) for removing saved settings.

## Privacy and support

Stat Wisp works locally: no telemetry, accounts, update checks, or outbound network requests. Network metrics read local Windows counters. It does not install sensor drivers or change hardware settings; external sensor tools have their own requirements.

Read the [troubleshooting guide](docs/user-guide.md#troubleshooting) for missing sensors or icons. For a reproducible problem, [open an issue](https://github.com/grayfvll01/stat-wisp/issues/new/choose) with your version, Windows build, hardware, and steps.

Free and open source under the [MIT License](LICENSE). [Build from source](docs/development.md) · [Contribute](CONTRIBUTING.md).

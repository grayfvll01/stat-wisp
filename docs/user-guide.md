# Using Stat Wisp

[Download releases](https://github.com/grayfvll01/stat-wisp/releases/latest) · [Sensor support](sensor-support.md) · [Back to the project](../README.md)

## Install and start

Stat Wisp runs on Windows 10/11 x64 as your current Windows user.

For a normal installation, download `stat-wisp-<version>-setup.exe` from a GitHub release and run it. The default installation folder is `%LOCALAPPDATA%\Programs\stat-wisp`. The installer adds Start menu and uninstall entries.

For use without installation, download `stat-wisp-<version>-win-x64.zip`, extract all files into a folder you intend to keep, and run `stat-wisp.exe`. Keep `LICENSE.txt` with the executable when redistributing it. The portable edition stores preferences in your Windows user profile; it does not store them beside the executable.

The first setup window asks which metrics to show. Select at least one, choose an update interval, and click **Start**. **CPU Usage** and **RAM Usage** are useful starting points that do not need an external sensor monitor. Stat Wisp then runs in the notification area; there is no main dashboard window.

Open the overflow beside the Windows clock if you do not see the icons. Windows controls which icons remain visible. Hover over an icon to see its full metric name, value, and units.

## Tray controls

Right-click any Stat Wisp icon to open the shared menu.

| Control | What it does |
|---|---|
| **Metrics** | Add or remove metric icons. At least one must remain enabled. |
| **Update interval** | Choose 500 ms, 1, 2, 5, 10, or 30 seconds, or one minute. |
| **Display** | Choose Celsius/Fahrenheit, hide the short labels with **Compact labels**, or round applicable values. |
| **Start with Windows** | Add or remove a startup entry for your current Windows account. |
| **Pause monitoring / Resume monitoring** | Stop or resume sampling. Paused icons retain their last readings. |
| **Restart monitoring** | Reconnect sensor providers and resume sampling. Some rates need a second sample. |
| **Settings…** | Change metrics and preferences together, then click **Save**. |
| **About** | See the installed version. |
| **Exit Stat Wisp** | Stop the application and remove its icons. |

Temperature icons are green below 60 °C, amber from 60 °C to below 80 °C, and red from 80 °C. The same thresholds apply when displaying Fahrenheit. These fixed colors are visual cues, not hardware-specific limits or temperature alerts.

Network and disk rates are bytes per second, using powers of 1,024 for `K`, `M`, and `G`. They are aggregate activity, not an internet speed test or per-application usage. Hover to see the units. CPU clock is the Windows-reported clock, which can differ from an effective-clock reading in another tool.

External WMI sensors are sampled at most once every two seconds, even with a shorter tray update interval. A longer interval can reduce polling work. Only enabled metric families are requested, although a provider may read several values together.

## CPU temperature and fan readings

CPU temperature and CPU/system fan RPM need a separate monitor exposing compatible sensors through WMI. Stat Wisp can read the local `ROOT\LibreHardwareMonitor` and `ROOT\OpenHardwareMonitor` namespaces.

1. Run a compatible [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) or [OpenHardwareMonitor](https://openhardwaremonitor.org/) build and enable its WMI publishing if that build provides a switch.
2. Check that the external monitor itself shows live values for your CPU or fans. That tool may need administrator rights for its hardware access; Stat Wisp runs as your normal user.
3. Keep the provider running, enable the desired metric in Stat Wisp, and choose **Restart monitoring**.

Stat Wisp prefers CPU package/die readings when exposed and can fall back to another recognized CPU temperature sensor. It does not treat firmware thermal zones as CPU temperature. Fan identification depends on the names published by the external monitor: the CPU fan needs a CPU name match, and the system fan reading is the fastest other recognized non-GPU fan.

If the provider cannot expose the sensor, Stat Wisp displays `--`. Installing Stat Wisp does not add hardware support to the external provider. See the [support matrix](sensor-support.md) for GPU and other limitations.

## Update

There is no automatic updater or in-app release check. Download updates from [GitHub Releases](https://github.com/grayfvll01/stat-wisp/releases/latest).

1. Right-click an icon and choose **Exit Stat Wisp**.
2. For an installed copy, run the new installer using the existing installation folder. For a portable copy, replace the files in its existing folder with the new ZIP's contents.
3. Launch Stat Wisp. Existing saved preferences are reused.

The installed and portable editions share `%LOCALAPPDATA%\stat-wisp\settings.ini`. Back up this file if you want to preserve a particular configuration. If you move a portable copy or switch installation folders, toggle **Start with Windows** off and on from the copy you want to use; the startup entry records the executable's full path.

## Uninstall

For an installed copy, exit Stat Wisp and remove it through Windows' installed apps list or **Uninstall Stat Wisp** in the Start menu. Uninstall removes the application's startup entry. Interactive uninstall asks whether to keep your settings for a future installation; silent uninstall retains them.

For a portable copy, turn off **Start with Windows**, choose **Exit Stat Wisp**, and delete its extracted folder. To remove saved preferences as well, delete `%LOCALAPPDATA%\stat-wisp` after exiting. This preferences folder is shared by installed and portable copies in the same Windows account.

Removing Stat Wisp does not remove an external hardware monitor you installed separately.

## Verify a download

Download the desired asset and `SHA256SUMS.txt` from the same release. In PowerShell, calculate the file's SHA-256 hash; replace `<version>` with the version you downloaded:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath "$env:USERPROFILE\Downloads\stat-wisp-<version>-setup.exe"
```

Compare the result with the line for the exact filename in `SHA256SUMS.txt`. Letter case does not matter. Matching hashes confirm that the file matches the published asset; checksums are not a code-signing certificate. Do not run a download whose hash differs.

Current release packages are unsigned. Windows may show a reputation warning or an unknown publisher. Use the assets attached to the official [stat-wisp repository's releases](https://github.com/grayfvll01/stat-wisp/releases), and follow your device or organization's software policy if installation is blocked.

## Troubleshooting

| Symptom | What to check |
|---|---|
| No icons after starting | Check the tray overflow. At first setup, select a metric and click **Start**. Launching another copy brings up the running copy's tray menu. |
| A reading is `--` | Wait for another sample, then check [sensor support](sensor-support.md). Unavailable is different from zero. |
| CPU temperature or fan RPM is missing | Confirm a compatible WMI provider is running and shows the sensor itself, then choose **Restart monitoring**. |
| GPU temperature, clock, fan, or VRAM is missing | Check vendor support. NVIDIA/AMD capabilities differ, and Windows GPU usage alone does not imply support for the other readings. |
| GPU usage differs from another tool | The Windows fallback sums process usage per physical engine, then uses the busiest engine across adapters. Sampling times, vendor readings, and adapter selection can differ. |
| Readings stopped changing | Check **Resume monitoring**, then try **Restart monitoring**. An external provider can also return stale values. |
| Startup stopped working after moving the app | Toggle **Start with Windows** off and on in the new location. |
| Preferences are not kept | Look for a save warning and check that `%LOCALAPPDATA%\stat-wisp` is writable. |
| Setup reappears on launch | Missing or invalid settings reopen first setup. Exit and back up the settings file before resetting it. |
| Preferences need a reset | Exit Stat Wisp, rename `%LOCALAPPDATA%\stat-wisp\settings.ini` to `settings.ini.bak`, and launch again. Complete first setup to save a fresh configuration. |

If the problem persists, [open an issue](https://github.com/grayfvll01/stat-wisp/issues/new/choose). Include the Stat Wisp version, Windows version/build, CPU and GPU model, driver version when relevant, selected metrics, update interval, and steps to reproduce. For a missing WMI sensor, include the external provider's name/version and whether it shows a live reading. Review screenshots and attachments for personal information before posting.

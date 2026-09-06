# Architecture

## Lifecycle and UI

`wWinMain` opts into per-monitor-v2 DPI awareness and enforces a per-user single instance. After first-run configuration, `Application` creates a one-pixel, non-activating, hidden top-level Win32 window. It is intentionally a top-level window rather than `HWND_MESSAGE` so it receives broadcast messages such as `TaskbarCreated`, power changes, theme changes, and display changes. It is never shown and has `WS_EX_TOOLWINDOW`, so normal operation has no taskbar button or primary window.

The first-run and Settings surfaces are small, native, modal Win32 windows created only on request. First run cannot be accepted with zero metrics. Configuration happens on first launch, so silent installation never waits for an interactive dialog.

Each enabled metric is a separate `Shell_NotifyIcon` entry owned by the same hidden window and process. Every icon opens one shared native menu. Explorer recovery uses the registered `TaskbarCreated` message to add the current set again. Shutdown deletes all entries before the window/process exits.

## Polling and provider activation

There is one monitoring `std::jthread` in addition to the UI thread. A condition variable implements the 500 ms, 1/2/5/10/30 second, and 1-minute schedule and wakes immediately for settings, pause, resume, restart, or shutdown. There is no spin loop. Samples move to the UI through a mutex-protected latest-value slot plus a posted window message, so polling does not allocate a message payload.

`Settings::RequiredProviders` is recalculated for every configuration. A provider is queried only while at least one enabled metric needs it. A configuration change resets provider state on the monitoring thread; disabling the final metric in a family stops its queries and unloads dynamic GPU libraries. Related values share a provider call where the API permits it (for example AMD activity returns load and clock together).

Hardware sensors are the exception to ordinary direct Windows data. The sensor provider is created lazily and reads no more than once every two seconds. It checks only the LibreHardwareMonitor and OpenHardwareMonitor WMI namespaces. GPU-only temperature configurations skip WMI when the driver has already returned a value. Missing external namespaces are retried periodically so starting a compatible monitor later works without restarting stat-wisp.

## Native providers

- CPU usage: `GetSystemTimes` deltas.
- CPU clock: average `CurrentMhz` from `CallNtPowerInformation(ProcessorInformation)`. This is the OS-reported current frequency, not an effective clock.
- RAM load, available RAM, and commit usage: one `GlobalMemoryStatusEx` query.
- Network: aggregate octet deltas from active, connected, non-loopback `GetIfTable2` rows.
- Disk rates/activity: one demand-driven PhysicalDisk PDH query.
- Battery percentage: `GetSystemPowerStatus`.
- CPU temperature: LibreHardwareMonitor/OpenHardwareMonitor WMI package/die sensors. Thermal-zone and ACPI readings are not used.
- Cross-vendor GPU usage fallback: the busiest Windows `GPU Engine` PDH counter.
- NVIDIA: dynamically loaded NVML from the installed display driver, including graphics and memory clocks and fan percentage.
- AMD: dynamically loaded ADL2 Overdrive 5 compatibility entry points from the installed display driver, including engine and memory clocks and fan percentage.
- CPU/system fans: RPM sensors exposed by a running LibreHardwareMonitor or OpenHardwareMonitor instance.

Dynamic providers never download or bundle a vendor DLL. Failure leaves the affected values unavailable and does not stop other metrics. Resume and the Restart command discard provider handles so display-driver restarts can recover cleanly.

## Rendering

`TrayRenderer` retains one 32-bit DIB section and memory DC for the current system small-icon size and DPI. Labels and values use a fixed 3×5 bitmap alphabet, scaled only to whole logical pixels; no small-font antialiasing is involved. `CreateIconIndirect` produces the shell icon, and every replaced `HICON` is explicitly destroyed. Temperature backgrounds use green below 60°C, amber from 60–79°C, and red at 80°C or above.

`TrayManager` caches label, formatted value, tooltip, and renderer generation. It calls `NIM_MODIFY` only when that key changes. Ten unchanged polling cycles therefore produce no icon drawing or shell update. A renderer generation change covers DPI, taskbar theme, and display option changes. Solid high-contrast category colors make the glyphs readable on both light and dark taskbars.

Windows, not applications, decides whether a notification icon is initially in the visible row or the overflow area. The application creates one valid icon per enabled metric but does not modify the user's notification-area preferences.

## Settings and startup

Settings are a small versioned UTF-8/ASCII INI-style file at `%LOCALAPPDATA%\\stat-wisp\\settings.ini`. The custom codec has no JSON dependency, ignores unknown keys for forward compatibility, validates intervals/order, and supports version migration. Writes happen only after a user change and use a same-directory temporary file plus `MoveFileEx(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.

Start with Windows is the quoted executable path in `HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run`. No task, service, administrator privilege, IPC, or background updater is used.

## Resource decisions

The Release target statically links the MSVC runtime and otherwise uses only Windows DLLs already in the operating system or display driver. There is no GUI framework, CRT redistributable requirement, bundled sensor library, GPL code, driver, telemetry, network client, or persistent logging thread. Major third-party dependencies are therefore zero.

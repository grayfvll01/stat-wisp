# Deploying Stat Wisp

[Back to the project](../README.md) · [User guide](user-guide.md)

## Silent install and uninstall

Per user (default, no administrator rights), installed to `%LOCALAPPDATA%\Programs\stat-wisp`:

```powershell
stat-wisp-1.0.0-setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
```

All users (administrator), installed to `Program Files\stat-wisp`:

```powershell
stat-wisp-1.0.0-setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /ALLUSERS
```

Uninstall silently with `unins000.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART` from the installation folder. Setup and uninstall close a running copy cleanly first. Silent uninstall keeps each user's settings.

## Disable updates

Stat Wisp checks `https://github.com/grayfvll01/stat-wisp/releases/latest` once a day. To turn checks and in-app installs off for managed machines, set this policy value (machine-wide or per user):

```powershell
reg add "HKLM\SOFTWARE\Policies\StatWisp" /v DisableUpdates /t REG_DWORD /d 1 /f
```

`HKCU\SOFTWARE\Policies\StatWisp` works the same way per user. The Settings window and tray menu then show updates as managed by your organization. For a machine-wide install, users without administrator rights cannot complete in-app updates; deploy new versions centrally instead.

If you allow updates through a firewall or proxy, permit HTTPS to `github.com` and `*.githubusercontent.com`. Stat Wisp uses the system proxy settings.

## Preconfigure settings

Each user's settings live in `%LOCALAPPDATA%\stat-wisp\settings.ini`. Deploying this file skips first setup:

```ini
version=3
enabled=cpu.usage,ram.usage,gpu.temperature
interval_ms=2000
fahrenheit=0
compact_labels=0
round_values=1
start_with_windows=0
check_for_updates=1
first_run_completed=1
```

Metric keys: `cpu.temperature`, `gpu.temperature`, `cpu.usage`, `gpu.usage`, `ram.usage`, `ram.available`, `memory.commit`, `cpu.clock`, `gpu.clock`, `gpu.memory_clock`, `vram.usage`, `network.download`, `network.upload`, `disk.read`, `disk.write`, `disk.usage`, `battery.percentage`, `cpu.fan`, `gpu.fan`, `system.fan`. Intervals: 500, 1000, 2000, 5000, 10000, 30000, or 60000 ms.

**Start with Windows** is a per-user `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` value named `stat-wisp` containing the quoted executable path.

## Files

| Path | Contents |
|---|---|
| `%LOCALAPPDATA%\stat-wisp\settings.ini` | Settings |
| `%LOCALAPPDATA%\stat-wisp\stat-wisp.log` | Diagnostic log, capped at 256 KiB (one previous file kept) |
| `%LOCALAPPDATA%\stat-wisp\crash-*.dmp` | Crash reports, newest three kept |
| `%LOCALAPPDATA%\stat-wisp\updates\` | Temporary update downloads, removed after use |

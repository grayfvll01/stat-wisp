# gate-monitor

gate-monitor is a small Windows tray utility that shows temperatures, fan speeds, usage, clocks, memory, network, disk, and battery readings as notification-area indicators. Temperature icons are green, amber, or red as they get hotter.

It is built with C++23 and native Win32 APIs. It reads Windows APIs and performance counters, NVIDIA NVML or AMD ADL, and LibreHardwareMonitor/OpenHardwareMonitor WMI sensors when either monitor is running. There is no browser runtime, telemetry, or network service.

## Use

1. Download and run `gate-monitor.exe`.
2. Select at least one metric.
3. Right-click any gate-monitor tray icon to change metrics, interval, display, startup, or exit.

Unsupported readings are shown as `--`. CPU temperature and motherboard fan access varies by PC firmware; running LibreHardwareMonitor or OpenHardwareMonitor adds a compatible sensor source. Windows may initially place new icons in the tray overflow area.

## Verify

Compare the release SHA-256 value with:

```powershell
Get-FileHash .\gate-monitor.exe -Algorithm SHA256
```

## Build

Requires Visual Studio with Desktop C++ and CMake:

```powershell
.\scripts\build-release.ps1
```

MIT licensed.

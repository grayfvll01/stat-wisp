# Sensor support

gate-monitor reports only values returned by the operating system, an already-installed display-driver API, or a compatible monitor already exposing WMI sensors. `--` means the selected metric is unsupported or temporarily unavailable; it is not a zero reading.

| Metric | Windows native | NVIDIA NVML | AMD ADL | LHM/OHM WMI | Notes |
|---|---:|---:|---:|---:|---|
| CPU usage | Yes | — | — | — | `GetSystemTimes`; all logical processors |
| CPU reported clock | Yes | — | — | — | Average OS `CurrentMhz`; not effective clock |
| CPU temperature | Limited | — | — | Yes | Package/die sensor preferred; thermal-zone fallback is firmware-dependent |
| CPU/system fan RPM | No | — | — | Yes | CPU name matching plus the fastest other exposed motherboard fan |
| RAM usage | Yes | — | — | — | Physical memory load |
| Available RAM | Yes | — | — | — | Available physical memory |
| Commit usage | Yes | — | — | — | Used percentage of the Windows commit limit |
| Network down/up | Yes | — | — | — | Active non-loopback interface totals |
| Disk read/write | Yes | — | — | — | Aggregate PhysicalDisk PDH rates |
| Disk activity | Yes | — | — | — | Aggregate PhysicalDisk `% Disk Time`, clamped to 100% |
| Battery | Yes | — | — | — | `GetSystemPowerStatus`; unavailable on desktops |
| GPU usage | WDDM fallback | Yes | Yes | — | WDDM value is the busiest exposed GPU engine across adapters |
| GPU temperature | No | Yes | Partial | Fallback | AMD requires legacy-compatible ADL Overdrive 5 entry points |
| GPU fan percentage | No | Yes | Partial | — | Vendor-reported requested/actual percentage; zero-RPM mode can report 0 |
| GPU core clock | No | Yes | Partial | No | Driver-reported current graphics/engine clock |
| GPU memory clock | No | Yes | Partial | No | Driver-reported current memory clock |
| VRAM usage | No | Yes | No | — | Percentage of first NVIDIA device memory |

## Vendor behavior

### NVIDIA

The application loads `nvml.dll` dynamically from the normal Windows/driver locations and monitors the first NVML device. Temperature, utilization, graphics clock, memory clock, memory use, and fan percentage are implemented. NVML is never shipped or downloaded by gate-monitor.

### AMD

The application loads `atiadlxx.dll` dynamically and selects the first present AMD adapter exposing ADL Overdrive version 5 or later compatibility calls. Temperature, engine activity, engine clock, memory clock, and fan percentage are implemented. Newer drivers that remove those compatibility calls still receive the Windows WDDM GPU-usage fallback, but the other values remain unavailable unless the WMI sensor fallback supplies temperature.

### Intel

Intel GPU utilization is available when the WDDM `GPU Engine` counters exist. Intel temperature, clock, and VRAM are not implemented because Windows provides no small stable public API for them and gate-monitor does not bundle a vendor runtime or privileged driver.

## CPU temperature limitation

Windows does not expose a universal CPU package-temperature API. gate-monitor first consumes package/die sensors from the `ROOT\\LibreHardwareMonitor` or `ROOT\\OpenHardwareMonitor` WMI namespace when present. It otherwise uses the hottest plausible Windows thermal-zone performance-counter reading and then ACPI WMI. Firmware zones may represent a board or chassis region rather than the CPU package. gate-monitor does not bundle a kernel driver; direct on-die access therefore still depends on a compatible monitor running with its own hardware access enabled.

## Multiple GPUs and adapters

Version 0.3.0 uses the first detected NVIDIA or supported AMD vendor device for vendor-specific metrics. The WDDM fallback reports the busiest engine across all adapters. Explicit adapter selection and multi-GPU-per-metric icons remain unsupported.

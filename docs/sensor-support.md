# Sensor support

Stat Wisp reports only values returned by the operating system, an already-installed display-driver API, or a compatible monitor already exposing WMI sensors. `--` means the selected metric is unsupported or temporarily unavailable; it is not a zero reading. See the [user guide](user-guide.md#cpu-temperature-and-fan-readings) for external sensor setup.

| Metric | Windows native | NVIDIA NVML | AMD ADL | LHM/OHM WMI | Notes |
|---|---:|---:|---:|---:|---|
| CPU usage | Yes | — | — | — | Windows `GetSystemTimes` deltas; processor-group limits apply on large systems |
| CPU reported clock | Yes | — | — | — | Average OS `CurrentMhz`; not effective clock |
| CPU temperature | No | — | — | Yes | Requires a running compatible CPU sensor provider; package/die readings are preferred |
| CPU/system fan RPM | No | — | — | Yes | CPU name matching plus the fastest other exposed motherboard fan |
| RAM usage | Yes | — | — | — | Physical memory load |
| Available RAM | Yes | — | — | — | Available physical memory |
| Commit usage | Yes | — | — | — | Used percentage of the Windows commit limit |
| Network down/up | Yes | — | — | — | Connected interface totals, excluding loopback and tunnel interface types |
| Disk read/write | Yes | — | — | — | Aggregate PhysicalDisk PDH rates |
| Disk activity | Yes | — | — | — | Aggregate PhysicalDisk `% Disk Time`, clamped to 100% |
| Battery | Yes | — | — | — | `GetSystemPowerStatus`; unavailable on desktops |
| GPU usage | WDDM fallback | Yes | Yes | — | WDDM sums process counters per physical engine, then uses the busiest engine |
| GPU temperature | No | Yes | Partial | Fallback | AMD requires legacy-compatible ADL Overdrive 5 entry points |
| GPU fan percentage | No | Yes | Partial | — | Vendor-reported requested/actual percentage; zero-RPM mode can report 0 |
| GPU core clock | No | Yes | Partial | No | Driver-reported current graphics/engine clock |
| GPU memory clock | No | Yes | Partial | No | Driver-reported current memory clock |
| VRAM usage | No | Yes | No | — | Percentage of first NVIDIA device memory |

## Vendor behavior

### NVIDIA

The application loads `nvml.dll` dynamically from System32 or the NVIDIA installation-path fallback and monitors the first NVML device. Temperature, utilization, graphics clock, memory clock, memory use, and fan percentage are implemented when the device and driver expose them. NVML is never shipped or downloaded by Stat Wisp.

### AMD

The application loads `atiadlxx.dll` dynamically and selects the first present AMD adapter reporting ADL Overdrive version 5. Temperature, engine activity, engine clock, memory clock, and fan percentage are implemented for that interface. Overdrive 6, Overdrive Next, and Overdrive 8 backends are not implemented. Other AMD adapters can still use the Windows WDDM GPU-usage fallback, but the other values remain unavailable unless the WMI sensor fallback supplies temperature.

### Intel

Intel GPU utilization is available when the WDDM `GPU Engine` counters exist. There is no Intel-specific backend for temperature, clocks, fan percentage, or VRAM. A recognized GPU temperature exposed by an external LHM/OHM WMI provider can supply the generic temperature fallback; its availability depends on that provider and sensor names.

## CPU temperature limitation

Windows does not expose a universal CPU package-temperature API. Stat Wisp consumes recognized CPU sensors from the `ROOT\LibreHardwareMonitor` or `ROOT\OpenHardwareMonitor` WMI namespace when present. Package, Tctl/Tdie, and die names are preferred, followed by average/max and other recognized CPU sensors. If neither provider supplies a CPU sensor, the reading is unavailable. Firmware thermal zones and ACPI are excluded because they may represent a board or chassis region and remain constant. Stat Wisp does not bundle a kernel driver; direct on-die access therefore still depends on a compatible monitor running with its own hardware access enabled.

WMI readings are cached for two seconds. The chosen tray refresh interval cannot make an external provider update faster. A provider can return stale or misidentified values; Stat Wisp cannot independently verify its hardware access.

## Multiple GPUs and adapters

Vendor-specific metrics use the first NVIDIA device when NVML initializes, otherwise the first supported AMD adapter. NVIDIA takes priority on a mixed NVIDIA/AMD system. The WDDM fallback adds process usage for each physical engine and reports the busiest engine across all adapters, capped at 100%. The external WMI temperature fallback selects a recognized GPU sensor by name and does not guarantee a match with the vendor-selected adapter. Explicit adapter selection and separate icons for each GPU remain unsupported.

## Aggregated readings

Network rates sum changes in counters for connected interfaces. A newly connected or reconnected interface needs a baseline sample before it contributes a rate. Virtual adapters can overlap physical traffic, so the total is not guaranteed to equal traffic on one internet connection. Disk rates and activity aggregate the exposed PhysicalDisk counters, and activity is clamped to 100%. These values can differ from tools that select a single adapter, disk, process, or GPU engine group.

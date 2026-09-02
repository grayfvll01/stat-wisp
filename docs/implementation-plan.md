# gate-monitor implementation plan

## Product boundaries

gate-monitor is a single-process, per-user, Windows 10/11 x64 Win32 application. It owns no persistent visible window: a non-activating hidden top-level window receives shell, power, and settings broadcasts; notification icons are the normal user interface; native configuration windows are created only for first run or an explicit Settings command.

The first release deliberately avoids drivers and bundled vendor runtimes. Sensor values are reported only when a Windows or already-installed display-driver API returns them. Unsupported values remain visibly unavailable (`--`) and are explained in the icon tooltip.

## Architecture and milestones

- [x] Establish a CMake C++23 `WIN32` target with manifest/version resources, strict warnings, a dependency-free core library, and pure logic tests.
- [x] Implement the metric catalog, formatting, settings codec/migration, `%LOCALAPPDATA%` persistence, and provider activation rules.
- [x] Implement one low-frequency monitoring worker with condition-variable scheduling and demand-driven native providers for CPU load/clock, memory, network throughput, ACPI thermal zones, Windows GPU performance counters, NVIDIA NVML, and AMD ADL.
- [x] Implement the hidden message window, Explorer restart recovery, sleep/resume handling, single-instance behavior, startup registry entry, and clean lifecycle.
- [x] Implement one notification icon per enabled metric, cached GDI rendering, DPI/theme refresh, native shared context menus, first-run selection, and the compact settings window.
- [x] Add the application icon and resources, Inno Setup installer, portable packaging, release workflow, concise public documentation, and support matrix.
- [x] Build and run Release x64 tests, launch isolated tray smoke tests, exercise repeated icon refresh and restart persistence, and record only measurements observed on this machine.

## Provider scope for 0.2.0

| Data | Backend | Scope |
|---|---|---|
| CPU usage | `GetSystemTimes` deltas | All supported Windows systems |
| CPU reported clock | `CallNtPowerInformation(ProcessorInformation)` | OS-reported current MHz; not effective clock |
| RAM | `GlobalMemoryStatusEx` | All supported Windows systems |
| Available RAM / commit usage | `GlobalMemoryStatusEx` | All supported Windows systems |
| Network rates | `GetIfTable2` octet deltas | Active, non-loopback interfaces |
| Disk rates / activity | Windows PhysicalDisk PDH counters | Counter availability-dependent |
| Battery percentage | `GetSystemPowerStatus` | Systems with a battery |
| CPU temperature | `MSAcpi_ThermalZoneTemperature` in `ROOT\\WMI` | Firmware/ACPI-dependent; not assumed to be a package sensor |
| GPU usage | Windows `GPU Engine` PDH counters | Driver/WDDM-dependent; busiest engine across installed adapters |
| NVIDIA temperature/usage/clock/VRAM | Dynamically loaded NVML from the installed NVIDIA driver | NVIDIA only; first detected device |
| AMD temperature/usage/clock | Dynamically loaded ADL Overdrive 5 entry points | Radeon drivers exposing those compatibility APIs |
| Intel temperature/clock/VRAM | None in 0.2.0 | GPU usage can still use WDDM counters; unavailable values are explicit |

No GPL sensor implementation, kernel driver, network access, telemetry, WMI polling loop for ordinary OS metrics, GUI framework, or runtime redistributable dependency will be introduced.

## Verification gates

- A clean Release x64 build and `ctest` run must pass.
- The executable must use the Windows subsystem and must not create a console or taskbar window during normal operation.
- Zero enabled metrics must be rejected in first run, settings, and runtime menu paths.
- Repeated unchanged samples must not regenerate icons; replaced `HICON`, bitmap, font, menu, provider, and COM resources must have explicit lifetime ownership.
- Provider activation tests must prove that disabled metric families are not requested.
- Manual smoke checks will cover initial configuration, tray menu, enable/disable, pause/restart, persistence, startup entry, Explorer recreation handling, and clean exit where this session permits automation.

# Resource verification

Use a Release x64 build and wait at least 60 seconds after startup and provider initialization. Record the Windows build, CPU/GPU hardware, driver versions, enabled metrics, and update interval with every result.

## Procedure

1. Start gate-monitor with the desired saved configuration.
2. Capture private working set, CPU time, thread count, handle count, and GDI objects at the start and after a timed interval.
3. Compute average CPU from the process CPU-time delta divided by elapsed wall time and logical processor count.
4. Use Windows Performance Recorder/Analyzer when wakeup attribution is needed; do not infer wakeups from the timer interval alone.
5. Toggle metrics repeatedly for five minutes and confirm handle/GDI counts return to a stable baseline.
6. Restart Explorer and verify every selected icon is restored. Suspend/resume the system and verify providers resume.

Suggested PowerShell observation (replace the process selection if more than one build is running):

```powershell
$p = Get-Process gate-monitor
$logical = [Environment]::ProcessorCount
$startCpu = $p.TotalProcessorTime.TotalSeconds
$start = Get-Date
Start-Sleep -Seconds 60
$p.Refresh()
$elapsed = ((Get-Date) - $start).TotalSeconds
[pscustomobject]@{
  AverageCpuPercent = 100 * ($p.TotalProcessorTime.TotalSeconds - $startCpu) / ($elapsed * $logical)
  PrivateMB = $p.PrivateMemorySize64 / 1MB
  WorkingSetMB = $p.WorkingSet64 / 1MB
  Threads = $p.Threads.Count
  Handles = $p.HandleCount
}
```

## Configurations

- Minimal: CPU temperature + GPU temperature.
- Typical: both temperatures + CPU/GPU usage + RAM.
- Heavy: all implemented metrics.

## Observed 0.1.0 development results

These are observations, not guarantees. They were measured on Windows 11 build 26200, an Intel Core i7-14700F (28 logical processors), an NVIDIA GeForce RTX 5070 Ti with driver 32.0.16.1656, a Release x64 build, a 1-second interval, eight seconds of warm-up, and a 20-second CPU sample:

| Configuration | Startup to hidden window | Average CPU | Private bytes | Working set | Threads | Handles | GDI objects |
|---|---:|---:|---:|---:|---:|---:|---:|
| Minimal | 187 ms | below sample resolution | 22.12 MB | 39.02 MB | 8 | 224 | 17 |
| Typical | 82 ms | below sample resolution | 22.08 MB | 39.10 MB | 8 | 224 | 26 |
| Heavy | 90 ms | 0.01% | 22.21 MB | 39.25 MB | 8 | 225 | 41 |

Private memory with CPU temperature enabled is slightly above the aspirational 20 MB target because the firmware thermal provider loads Windows WMI/RPC infrastructure. The final 387,584-byte executable itself has two application-owned threads; the extra observed threads and most of the resident/private cost appear after the ACPI WMI provider initializes. This tradeoff is retained because it is the only implemented driverless CPU thermal path and it sleeps between five-second reads.

After 100 forced theme/display refreshes in the heavy configuration, handles remained 225 and GDI objects remained 41. This specifically exercises destruction and replacement of the cached DIB/fonts and all ten `HICON` values.

An extended 0.2.0 smoke run with all 17 metrics enabled at a 10-second interval used 26.53 MB private memory and nine observed threads after provider initialization. Handles remained 323 and GDI objects remained 62 after Explorer-recreation simulation and 50 forced icon refreshes; the process then exited cleanly.

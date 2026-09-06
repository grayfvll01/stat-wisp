# Resource measurements

Short local comparison on Windows 11 build 26200, Intel Core i7-14700F and NVIDIA RTX 5070 Ti, September 6, 2026. Both builds used a one-second polling interval, eight-second warm-up, and twelve-second sample. These are observations, not a long-duration leak or performance guarantee.

| Configuration | Previous build private memory | Stat Wisp 0.4.0 private memory | Stat Wisp working set |
|---|---:|---:|---:|
| CPU usage + RAM usage | 1.86 MiB | 1.92 MiB | 13.39 MiB |
| CPU + GPU temperature | 25.45 MiB | 21.88 MiB | 37.57 MiB |

The temperature configuration used approximately 14% less private memory. It no longer creates firmware thermal-zone/ACPI queries. CPU temperature was unavailable because no compatible external WMI sensor provider was running. GPU readings came from NVML. CPU/RAM-only memory was essentially unchanged.

GDI objects remained 14 throughout each sample. The runtime regression test also replaced 1,000 temperature icons with stable GDI object counts and verified that cool/warm/hot images change and repeat correctly. CPU time in the new builds was below the short sample's resolution; this does not imply zero CPU use.

Normal Exit completed in 10 ms in the regression test. With a deliberately stalled provider it completed in 3,022 ms using the bounded shutdown fallback.

For longer measurements, run a Release build for at least a minute before sampling, record private bytes, working set, GDI objects, handles, and process CPU-time deltas. Compare identical configurations and intervals. External sensor services and GPU drivers add their own memory and can dominate temperature monitoring costs. Do not force working-set trimming: it hides resident usage by paging rather than reducing allocations.

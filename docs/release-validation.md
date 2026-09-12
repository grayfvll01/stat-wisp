# Stat Wisp 0.4.1 release review

Reviewed and built on September 12, 2026. The clean local Release build and the checks below passed. No unresolved release blocker was identified within this review's scope; hardware coverage is limited as described below.

## Findings addressed

| Area | Finding and resulting behavior |
|---|---|
| AMD detection | Corrected the ADL vendor value and restricted Overdrive 5 calls to adapters reporting that interface. Newer interfaces retain available Windows/WMI fallbacks. |
| Windows GPU usage | Sum process contributions for each physical engine before choosing the busiest engine; separate adapters and engines remain separate. |
| Network rates | Track baselines by interface identity so connections, disconnections, ordering changes, and counter resets do not introduce lifetime-counter spikes. |
| Settings persistence | Detect buffered write/close failures before replacing the previous settings; report first-setup save failures, accept relative override paths, and reject malformed version fields. |
| Settings dialog | Prevent duplicate dialogs and competing tray commands while edits are open; verify Save, reopen, and application Exit. |
| Startup preference | Change the per-user startup entry only when its preference changes. |
| Release pipeline | Perform clean builds, require tests, validate versions and package integrity, exercise installer lifecycle in CI, and use checked-in release notes for the draft. |

## Local validation

Environment: Windows 11 Pro build 26200, Visual Studio 2026 C++ tools, Windows SDK 10.0.26100.0, and Inno Setup 6.7.3. The local GPU is an NVIDIA GeForce RTX 5070 Ti.

```powershell
./scripts/build-release.ps1 -Version 0.4.1
./scripts/test-release.ps1 -Version 0.4.1 -Install -PreviousInstaller ./out/release-review/published-v0.4.0/stat-wisp-0.4.0-setup.exe
```

- Clean x64 Release compilation completed without compiler warnings.
- All five CTest checks passed: core, runtime, normal Exit, stalled-provider Exit, and settings-dialog interaction.
- Core regressions cover network topology/counter changes and GPU aggregation across processes, engines, and adapters.
- Runtime checks cover settings persistence and replacement failures, invalid settings versions, 1,000 icon replacements without a GDI-object increase, and local NVIDIA temperature sampling.
- The normal Exit check completed successfully; the deliberately stalled worker caused the expected bounded shutdown with exit code 1460.
- Package checks verified all five release files, complete SHA-256 coverage, version resources, x64 architecture, ASLR/DEP/Control Flow Guard flags, and matching executable/license contents in the portable ZIP. A deliberately corrupted asset was rejected.
- Downloaded the actual published v0.4.0 installer and verified its published checksum before testing upgrade to v0.4.1. Upgrade, same-version repair, installed application launch/Exit, and silent uninstall passed.
- PowerShell files parse, documentation links resolve within the repository, issue forms parse as YAML, and Git whitespace checks pass.

Local CTest logs are under `out/stat-wisp-release/Testing/Temporary/`; installation logs are retained in isolated `out/package-test-*` folders. These generated files are not part of the source release. The [GitHub release workflow](../.github/workflows/release.yml) independently rebuilds the tag, runs all five checks plus fresh installation/repair/uninstall, and verifies downloaded artifacts before creating the draft. See [Actions](https://github.com/grayfvll01/stat-wisp/actions/workflows/release.yml) for the remote run record.

## Coverage limits

Windows 10, AMD/Intel hardware, mixed-GPU configurations, battery hardware, every WMI provider, and every driver version were not physically tested in this review. Synthetic regressions validate the changed aggregation logic; they do not establish hardware support beyond the [sensor matrix](sensor-support.md). Interactive installer dialogs, reboot/startup behavior, sleep/resume, and Explorer restart were not part of the automated checks above.

Release binaries are unsigned. No new performance benchmark or universal hardware-compatibility claim is made. The release workflow stages a draft with every asset; publication is a separate final action after reviewing its CI result and notes.

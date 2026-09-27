# Development and releases

Build on Windows with Visual Studio's Desktop development with C++ workload (including the Windows SDK), CMake 3.25+, PowerShell 7, and Inno Setup 6:

```powershell
./scripts/build-release.ps1          # clean build, all tests, installer, ZIP, update manifest, checksums
./scripts/build-release.ps1 -ExeOnly # executable only
```

The version comes from the `VERSION` file; CMake generates the version header and application manifest from it. Binaries and archives live only in ignored `dist/` and GitHub Releases.

The tests cover core logic, the update manifest and SHA-256 checks, Windows runtime behavior, settings-dialog isolation and saving, normal Exit, Exit with a deliberately stalled provider, and an end-to-end update (`scripts/test-update.ps1`) against a local HTTP server: a tampered download must be rejected and a verified one installed in place with a restart.

`scripts/test-release.ps1` checks the exact six release assets, the update manifest against the actual files, SHA-256 entries, versions, x64 machine type, memory-protection flags, the license, and the portable ZIP. Add `-Install` to exercise per-user install, repair, launch/Exit, and silent uninstall on a machine without Stat Wisp installed, and `-PreviousInstaller <path>` to test an upgrade.

`scripts/capture-settings.ps1` regenerates `docs/images/settings.png`.

## Release procedure

1. Set the new version in `VERSION` and move the `[Unreleased]` notes in `CHANGELOG.md` under a `## [X.Y.Z] - YYYY-MM-DD` heading.
2. Run `./scripts/build-release.ps1`, then merge to `main`.
3. Push a matching `vX.Y.Z` tag. CI rebuilds, tests, checks install/repair/uninstall, and creates a draft release with all assets and the changelog section as notes.
4. Review the draft and publish it. Publishing makes it the latest release, which in-app updates pick up within a day. Never move a published tag.

Workflow actions are pinned to commit hashes. Pull requests only receive read permission. Tag values are passed through environment variables, not interpolated into shell code. GitHub-hosted Windows runners must include Inno Setup 6; missing tools fail the build. Branch and pull-request builds validate artifacts; only matching tags create a draft release.

See [GitHub release management](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository) and [immutable releases](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases).

## Safety and limitations

The application runs as the current user, reads sensors, and writes only settings and an optional per-user startup entry. It does not install a driver, change hardware settings, or download executable code. GPU libraries are loaded from System32, with an explicit NVIDIA installation-path fallback. Settings reads are capped at 64 KiB. The executable enables ASLR, DEP, and Control Flow Guard.

WMI queries are bounded when enumerating results. On Exit, tray icons are removed, cooperative shutdown and COM cancellation are requested, and a provider that still refuses to finish after three seconds causes the application to terminate its own process. Settings are saved when changed, before shutdown. No individual thread is forcibly terminated.

CPU thermal-zone/ACPI values are deliberately excluded: those may be static motherboard/chassis readings, not CPU package temperature. A compatible external WMI sensor provider is required for CPU temperature, with package/die names preferred when present. External providers can themselves return stale data; the application cannot independently validate their hardware access.

Release binaries are unsigned unless a maintainer supplies a signing certificate. Windows may show a reputation warning. Source review and tests are not a guarantee that all hardware/driver combinations are supported.

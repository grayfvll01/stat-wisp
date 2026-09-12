# Development and releases

Build on Windows with Visual Studio's Desktop development with C++ workload (including the Windows SDK), CMake 3.25+, PowerShell, and Inno Setup 6:

```powershell
./scripts/build-release.ps1 -Version 0.4.1
```

Use `-ExeOnly` for a development build without the installer. The full build requires an installer; it never silently skips it. Binaries and archives live only in ignored `dist/` and GitHub Releases, not the source tree or README. The portable archive and installer include the complete MIT license.

The build checks the requested version against CMake, the version header's numeric/string fields, and the application manifest. It performs a clean Release x64 rebuild with tests enabled and fails if CTest finds no tests. The tests cover core behavior, Windows runtime behavior, settings-dialog isolation and saving, normal Exit, and Exit with a deliberately stalled provider.

A full build also runs `scripts/test-release.ps1`. This checks the exact five release assets, complete and matching SHA-256 entries, executable and installer versions, x64 machine type, executable memory-protection flags, the MIT license, and the portable ZIP's expected contents. The executable and license inside the ZIP must match the standalone files.

To repeat the package checks and exercise the installer on a disposable Windows account or clean machine:

```powershell
./scripts/test-release.ps1 -Version 0.4.1 -Install
```

The `-Install` check performs per-user installation, reinstalls the same package to check repair, launches the installed application and requests Exit, then silently uninstalls it. It refuses to run when an existing Stat Wisp installation, startup entry, or running process could be affected. Test installation files and logs are placed under an isolated `out/package-test-<id>` folder; the check still writes normal per-user installer registration. Omit `-Install` to verify packages without installing them. A repair check does not substitute for testing an upgrade from the previous published version.

To also test an upgrade, supply a downloaded installer from the preceding release:

```powershell
./scripts/test-release.ps1 -Version 0.4.1 -Install -PreviousInstaller ./out/stat-wisp-0.4.0-setup.exe
```

This installs the preceding version in the same isolated folder before installing and checking the new version.

## Release procedure

The [0.4.1 release review](release-validation.md) records completed checks and coverage limits.

1. Update CMake, `src/version.h` (numeric and string versions), the manifest, and packaging version defaults together.
2. Write `docs/release-notes-<version>.md` with the actual changes, download choices, upgrade instructions, and known limitations. Update user documentation for changed behavior.
3. Run the full Release build and package checks, then the installer smoke test in a clean environment. Smoke-test first setup, settings, tray controls, supported sensor readings, startup, and an upgrade from the preceding release. Record completed checks and untested hardware separately; do not reuse an older benchmark as new evidence.
4. Commit and push the reviewed source and release notes, then push a matching `vMAJOR.MINOR.PATCH` tag. Never move a published tag.
5. CI builds the executable, installer, portable archive, license, and SHA-256 checksums. It checks installation, repair, application Exit, and uninstall before retaining the assets.
6. A separate job with release-write permission downloads that run's assets, verifies the packages again, and creates a draft with the checked-in release notes. Missing notes or failed checks stop release creation.
7. Check the draft's assets, notes, CI results, and recorded validation before publishing the release. Upload every asset before publication; this also supports GitHub immutable releases.

Workflow actions are pinned to commit hashes. Pull requests only receive read permission. Tag values are passed through environment variables, not interpolated into shell code. GitHub-hosted Windows runners must include Inno Setup 6; missing tools fail the build. Branch and pull-request builds validate artifacts; only matching tags create a draft release.

See [GitHub release management](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository) and [immutable releases](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases).

## Safety and limitations

The application runs as the current user, reads sensors, and writes only settings and an optional per-user startup entry. It does not install a driver, change hardware settings, or download executable code. GPU libraries are loaded from System32, with an explicit NVIDIA installation-path fallback. Settings reads are capped at 64 KiB. The executable enables ASLR, DEP, and Control Flow Guard.

WMI queries are bounded when enumerating results. On Exit, tray icons are removed, cooperative shutdown and COM cancellation are requested, and a provider that still refuses to finish after three seconds causes the application to terminate its own process. Settings are saved when changed, before shutdown. No individual thread is forcibly terminated.

CPU thermal-zone/ACPI values are deliberately excluded: those may be static motherboard/chassis readings, not CPU package temperature. A compatible external WMI sensor provider is required for CPU temperature, with package/die names preferred when present. External providers can themselves return stale data; the application cannot independently validate their hardware access.

Release binaries are unsigned unless a maintainer supplies a signing certificate. Windows may show a reputation warning. Source review and tests are not a guarantee that all hardware/driver combinations are supported.

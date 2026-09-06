# Development and releases

Build on Windows with Visual Studio's Desktop development with C++ workload, CMake 3.25+, and Inno Setup 6:

```powershell
./scripts/build-release.ps1 -Version 0.4.0
```

Use `-ExeOnly` for a development build without the installer. The full build requires an installer; it never silently skips it. Binaries and archives live only in ignored `dist/` and GitHub Releases, not the source tree or README. The portable archive and installer include the complete MIT license.

## Release procedure

1. Update CMake, `src/version.h` (numeric and string versions), the manifest, and packaging version defaults together.
2. Run the Release build and tests. Check both normal Exit and the deliberately stalled provider regression. Smoke-test settings, sensor readings, and install/uninstall.
3. Commit and push the source, then push a matching `vMAJOR.MINOR.PATCH` tag. Never move a published tag.
4. CI builds the executable, installer, portable archive, license, and SHA-256 checksums. A separate job with write permission uploads all assets to a draft release.
5. Check the draft's assets and notes before publishing. This order also works with GitHub immutable releases: upload every asset before publication.

Workflow actions are pinned to commit hashes. Pull requests only receive read permission. Tag values are passed through environment variables, not interpolated into shell code. GitHub-hosted Windows runners must include Inno Setup 6; missing tools fail the build.

See [GitHub release management](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository) and [immutable releases](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases).

## Safety and limitations

The application runs as the current user, reads sensors, and writes only settings and an optional per-user startup entry. It does not install a driver, change hardware settings, or download executable code. GPU libraries are loaded from System32, with an explicit NVIDIA installation-path fallback. Settings reads are capped at 64 KiB. The executable enables ASLR, DEP, and Control Flow Guard.

WMI queries are bounded when enumerating results. On Exit, tray icons are removed, cooperative shutdown and COM cancellation are requested, and a provider that still refuses to finish after three seconds causes the application to terminate its own process. Settings are saved when changed, before shutdown. No individual thread is forcibly terminated.

CPU thermal-zone/ACPI values are deliberately excluded: those may be static motherboard/chassis readings, not CPU package temperature. A compatible external WMI sensor provider is required for CPU temperature. External providers can themselves return stale data; the application cannot independently validate their hardware access.

Release binaries are unsigned unless a maintainer supplies a signing certificate. Windows may show a reputation warning. Source review and tests are not a guarantee that all hardware/driver combinations are supported.

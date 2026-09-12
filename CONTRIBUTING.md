# Contributing to Stat Wisp

Bug reports, documentation corrections, and focused pull requests are welcome. Read the [user guide](docs/user-guide.md) and [sensor support matrix](docs/sensor-support.md) before reporting a missing reading.

## Report a problem or propose a feature

Search [existing issues](https://github.com/grayfvll01/stat-wisp/issues) first, then use the [issue forms](https://github.com/grayfvll01/stat-wisp/issues/new/choose). Describe the observed behavior, what you expected, and steps someone else can follow. Include the application version, Windows build, relevant hardware and drivers, and selected metrics.

For a feature request, explain the user problem and a concrete example. Hardware support depends on available Windows, display-driver, and external WMI interfaces. A request does not imply a delivery date.

Remove private information from logs and screenshots before posting. Do not attach credentials, personal files, or unrelated system dumps.

## Submit a change

1. Fork the repository and work on a branch for one coherent change.
2. Follow the [development guide](docs/development.md) to build and run the checks that apply to your change.
3. Keep sensor reads off the UI thread, request only needed providers, and show unavailable readings explicitly. Preserve the application's local, per-user operation.
4. Add regression coverage for behavior changes where it can detect the original failure. Update user documentation when behavior or supported sensors change.
5. Open a pull request describing the problem, resulting behavior, and checks you actually ran. List hardware-dependent behavior you could not verify.

Do not commit generated build folders, binaries, installers, or release archives. Existing source uses C++23, native Win32 APIs, and CMake; keep changes consistent with nearby code.

Contributions are provided under the repository's [MIT License](LICENSE).

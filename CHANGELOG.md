# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.1] - 2025-01-29

### Added

- Write logs to a temporary file in Debug builds.
- NSIS installer for the 32-bit and 64-bit versions, including an uninstaller
  and an autorun entry.
- Version resource for the executable.

### Changed

- Convert the solution to Visual Studio 2022.
- Exclude the auxiliary projects `ShiftSwitchUI2` and
  `ShiftSwitchEnumLayouts` from the batch build.

## [1.0.0.0] - 2022-07-18

### Added

- Switch the keyboard layout by tapping Shift: left Shift switches to English,
  right Shift switches to Russian.
- Hook DLL that is injected into 32-bit and 64-bit applications to handle the
  Shift keys in the foreground window.
- Tray application with Activate and Deactivate commands, single-instance
  protection and separate 32-bit and 64-bit builds.

[Unreleased]: https://github.com/alezhu/Shift-Switch/compare/v1.0.1...HEAD
[1.0.1]: https://github.com/alezhu/Shift-Switch/compare/v1.0.0.0...v1.0.1
[1.0.0.0]: https://github.com/alezhu/Shift-Switch/releases/tag/v1.0.0.0

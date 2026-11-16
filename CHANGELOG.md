# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Start Menu program folder and application shortcut during installation, with clean removal on uninstallation.

### Fixed

- Prevent duplicate instances from running simultaneously, including when launched from different folders or paths.
- Reliable keyboard layout switching and immediate taskbar indicator updates in applications with child controls and multi-window interfaces in Release builds.

## [1.1.1] - 2026-11-16

### Fixed

- Immediate update of the system taskbar language indicator when switching keyboard layouts, without requiring a window focus change.
- Reliable layout switching across 32-bit and 64-bit applications in Release builds.

## [1.1.0] - 2026-11-16

### Added

- Settings dialog to choose target keyboard layouts for Left Shift and Right Shift.
- Option to remember dialog window positions across launches on multi-monitor setups.
- Process exclusions list: disable layout switching in specific games or applications.
- Selection dialog to easily pick running applications to add to the exclusion list.
- Dynamic system tray icon visually indicating active vs inactive state.
- Global customizable hotkey to quickly enable or disable layout switching.
- Crash report generation to simplify troubleshooting in case of unexpected errors.
- Automatic startup option per user and clean uninstallation.
- Centered About dialog with project version and GitHub repository link.
- Automated test suite covering keyboard hooks, inter-process communication, and settings.

### Fixed

- Installer packaging reliability to ensure valid installation packages across all builds.
- Long-term background stability and timer accuracy during extended system uptime.
- Seamless layout switching in Start menu, Windows Search, and modern AppContainer applications.
- Synchronized language indicator in the Windows taskbar immediately after switching.
- Prevented application freezes when switching layouts in .NET and Qt applications.
- Reliable tray context menu positioning and dismissal when clicking outside on multi-monitor setups.
- Consistent hook coordination and state synchronization between 32-bit and 64-bit applications.
- Complete removal of autostart entries during uninstallation.

## [1.0.1] - 2025-01-29

### Added

- Diagnostic logging to a temporary file in Debug builds.
- NSIS installer for 32-bit and 64-bit versions with autostart support.
- Version metadata for the executable.

### Changed

- Updated project for Visual Studio 2022.
- Excluded auxiliary legacy projects from the main build.

## [1.0.0.0] - 2022-07-18

### Added

- Switch the keyboard layout by tapping Shift: Left Shift switches to English, Right Shift switches to Russian.
- Dual 32-bit and 64-bit hook DLL injection for foreground windows.
- System tray application with Activate and Deactivate commands.

[Unreleased]: https://github.com/alezhu/Shift-Switch/compare/v1.1.1...HEAD
[1.1.1]: https://github.com/alezhu/Shift-Switch/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/alezhu/Shift-Switch/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/alezhu/Shift-Switch/compare/v1.0.0.0...v1.0.1
[1.0.0.0]: https://github.com/alezhu/Shift-Switch/releases/tag/v1.0.0.0

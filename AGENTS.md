# Agent Rules

These rules apply to any AI agent working in this repository.

## Problem Solved & Architectural Invariants

ShiftSwitch was created to solve specific flaws in existing keyboard layout switchers (notably Punto Switcher, AltSwitcher, and similar tools):

1. **Eliminate Application Freezes/Hangs**:
   - In Punto Switcher and standard hook implementations, switching the layout causes certain applications to freeze completely — especially .NET applications (e.g., TheBrain) and Qt applications (e.g., HitFilm Express).
   - Calling `ActivateKeyboardLayout` synchronously inside a keyboard hook callback or sending `WM_INPUTLANGCHANGEREQUEST` causes deadlocks in these message loops.
   - **Invariant**: Layout activation must be executed asynchronously or decoupled from the hook callback (e.g., in a separate worker thread or via asynchronous message processing) to prevent target processes from hanging.

2. **Dedicated Single-Key Layout Switching**:
   - Left Shift switches to English (or the configured left layout); Right Shift switches to Russian (or the configured right layout).
   - Unlike cyclical toggle keys (like CapsLock or Win+Space), dedicated keys guarantee the target layout deterministically with a single press before typing, without requiring the user to verify the current layout or look at indicators.

3. **Preserve Modifier Behavior (No Swallowed Keys)**:
   - In alternatives like AltSwitcher, intercepting Shift can swallow the modifier, causing `Shift+<key>` combinations to only send `<key>` to the application.
   - **Invariant**: A layout switch is triggered **only** on a solitary tap-and-release of Shift (no other keys pressed while Shift is held down). Shift must never be consumed or swallowed when used in combinations (`Shift + Key`, `Shift + Click`, capitalization, text selection).

4. **Robust Cross-Bitness (x86/x64) Isolation**:
   - 64-bit processes cannot load 32-bit DLLs and vice versa. Windows cross-bitness `WH_KEYBOARD` hook marshalling routes hook callbacks back to the installer thread rather than the target process thread.
   - **Invariant**: The dual-process architecture (`ShiftSwitchUI.exe` for 32-bit and `ShiftSwitchUI64.exe` for 64-bit) and corresponding DLLs must be maintained. Target process bitness is checked explicitly; cross-process message delivery uses `WH_GETMESSAGE` hooks and custom messages permitted through UIPI via `ChangeWindowMessageFilter`.

## Commits

- One logical change per commit. Do not mix unrelated fixes, refactorings,
  formatting and documentation in a single commit.
- Split a task that contains several independent changes into separate commits.
- Commit messages are written in English, in the imperative mood, and use the
  conventional prefix used by this repository: `feat:`, `fix:`, `docs:`,
  `build:`, `chore:`, `refactor:`, `test:`.
- Keep the tree buildable at every commit.
- Never commit generated artifacts (`Debug/`, `Release/`, `x64/`, `publish/`,
  `packages/`, `*.exe`, `*.dll`, `*.aps`); they are already ignored.

## Changelog

- `CHANGELOG.md` is maintained in the Keep a Changelog format.
- Write entries from the end user's perspective rather than the developer's:
  describe user-visible behavior, stability, or packaging changes; avoid
  internal technical details, function/variable names, compiler warning codes,
  or implementation mechanics.
- Every commit that changes behaviour, build output or the public interface
  must update `CHANGELOG.md` in the same commit.
- Unreleased work goes under `## [Unreleased]` in the matching subsection:
  `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`.
- When a release is made, move the entries from `[Unreleased]` into a new
  `## [<version>] - <YYYY-MM-DD>` section.

## Releases

- A release is a separate commit that contains only release metadata: version
  bumps, the moved changelog section and release notes.
- The release commit is tagged `v<version>` (for example `v1.0.2`).
- Do not create a release commit or a tag together with regular changes.

### Version

- The version has three components, `MAJOR.MINOR.PATCH` (for example `1.0.1`).
  Do not add a fourth component.
- Bump `PATCH` when the release contains only fixes.
- Bump `MINOR` when functionality is added or changed.
- Bump `MAJOR` only on an explicit instruction from the user.
- `VERSION` is the single source of truth for the project version (e.g. `1.1.0`).
  All build systems (MSBuild, CMake, NSIS installer, resource scripts) derive their
  version definitions automatically from this file.
- Update every place below in the same release commit:
  - `VERSION`: the single-line version string (`MAJOR.MINOR.PATCH`).
  - `CHANGELOG.md`: the new `## [<version>] - <YYYY-MM-DD>` section.
  - The `v<version>` tag on the release commit.
- `ShiftSwitchUI2` is archived in `ShiftSwitch-Legacy.sln`, so its version resource is not updated on
  release.

## Git operations

- Do not commit, amend, tag or push unless the user explicitly asks for it in
  the current request.
- Never force-push, rewrite published history or change remotes without an
  explicit request.
- When asked to commit, stage only the files that belong to the logical change.

## Build

- Build with MSBuild, for example:
  `MSBuild ShiftSwitch.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145`
- Or build with CMake using presets:
  `cmake --preset x64-release`
  `cmake --build --preset x64-release`
  `ctest --preset x64-release`
  Available presets: `x64-release`, `x64-debug`, `x86-release`, `x86-debug`.
- Alternatively, run `build.cmd` (defaults to Debug) or `build.cmd release` to build all architectures via CMake.
- The solution defines the `x64` and `x86` platforms. Legacy projects
  (`ShiftSwitchUI2` and `ShiftSwitchEnumLayouts`) reside in
  `ShiftSwitch-Legacy.sln` and are not part of `ShiftSwitch.sln`.

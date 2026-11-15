# Shift Switch

Windows keyboard layout switcher between English and Russian (and configurable layouts) by dedicated Left and Right Shift keys.

Switch to English by pressing the left Shift key. Switch to Russian by pressing the right Shift key.

## The Problem ShiftSwitch Solves

Most popular keyboard layout switchers (such as Punto Switcher) suffer from a critical flaw: switching layouts causes certain applications to freeze completely. This issue frequently affects .NET applications (such as TheBrain) and Qt applications (such as HitFilm Express).

Other alternatives have major shortcomings:
- They lack dedicated single-key switching per layout, forcing users to cycle through layouts and constantly check system indicators.
- They swallow the `Shift` key (for example, in AltSwitcher, pressing `Shift+<key>` sends only `<key>` to the application, breaking hotkeys and combinations).
- They also trigger application hangs when attempting to switch layouts.

In standard WinAPI hooks:
- Calling `ActivateKeyboardLayout` synchronously inside a keyboard hook callback causes target processes (.NET/Qt) to deadlock.
- Sending `WM_INPUTLANGCHANGEREQUEST` via `PostMessage` similarly leads to freezes.

## Project Goals & Key Features

1. **Dedicated Key per Layout**:
   - **Left Shift** switches to English.
   - **Right Shift** switches to Russian.
   - Layouts can also be customized in Settings. Having a dedicated key for each language eliminates guessing or glancing at indicators: you tap the key and know with certainty which language will be typed.
2. **Zero Application Freezes**:
   - Prevents hangs and deadlocks in .NET, Qt, and other applications by activating layouts asynchronously (in a separate thread / decoupled from the hook callback).
3. **Full Modifier Transparency**:
   - Layout switches only on a clean tap-and-release of Shift alone.
   - When Shift is used in combination with other keys or mouse actions (`Shift + Key`, `Shift + Click`, capitalization, text selection), Shift functions normally as a modifier and is never swallowed.
4. **Seamless 32-bit & 64-bit Process Support**:
   - Uses paired 32-bit and 64-bit helper executables and hook DLLs to properly handle Windows cross-bitness hook marshalling and process boundaries without cross-bitness injection failures.

## Project Structure

- [ShiftSwitchUI](https://github.com/alezhu/Shift-Switch/tree/main/ShiftSwitchUI) — Main program project (Win32 GUI and tray icon)
- [ShiftSwitch](https://github.com/alezhu/Shift-Switch/tree/main/ShiftSwitch) — Hook DLL (`WH_KEYBOARD` and `WH_GETMESSAGE`)
- [ShiftSwitchEnumLayouts](https://github.com/alezhu/Shift-Switch/tree/main/ShiftSwitchEnumLayouts) — Test console project for layout enumeration (not used in build)
- [ShiftSwitchUI2](https://github.com/alezhu/Shift-Switch/tree/main/ShiftSwitchUI2) — Former WTL experiment (not used in build)

## Background & Origin

- [История появления (LiveJournal)](https://az-spb-ru.livejournal.com/46426.html)
- [История появления (Dreamwidth)](https://alezhu.dreamwidth.org/177.html)

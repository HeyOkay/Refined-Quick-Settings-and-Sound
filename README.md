# Refined Quick Settings and Sound

[Русский](docs/README.ru.md)

![Dark theme](docs/assets/preview-lossless.webp?v=01bbd42)

![Light theme](docs/assets/preview-light-lossless.webp)

This "vanilla+" Windhawk mod reworks the Windows 11 "Quick Settings" and "Volume mixer" panels and makes them fit for daily use. It follows the Windows 11 design language with thin sliders, even row spacing and a smooth frame transition to the Sound page.

Everything runs inside the shell's own panels (ShellHost). The system Acrylic, fonts, themes, lists, bindings and animations are kept, and the original layout is restored when the mod is unloaded.

## Features

**Quick Settings**

- Choose up to five **Additional shortcuts** to the right of **Settings**, including Notifications, Graphics, Advanced network settings, Power and Windows Update. Empty slots and duplicate shortcuts are skipped. The Display shortcut is also skipped when the brightness row already has a Display button.
- **Display brightness** uses the Windows slider in the default **Auto** mode and falls back to DDC/CI on compatible monitors with DDC/CI enabled. **Disable** hides the brightness row.
- The mod places Night light and Display settings beside the brightness slider. If the native slider cannot be moved, Windows keeps its original row. Night light uses the native quick action when available and opens Night light settings otherwise.
- On devices with a battery, the battery button moves into the **Settings** row.

- **Rows of tiles** sets 1–4 rows per page with Windows pagination. If the mod cannot recognize the tile grid layout, Windows uses its default row count.
- **Tile shape** rounds the outer edges of Quick Settings tiles in **Pills** mode.

**Sound**

- The "Sound output" page is now simply "Sound".
- Change the output and input devices and the spatial sound provider from dropdown lists.
- Below them is a reworked Volume mixer for the apps. It starts collapsed the first time Sound opens in each ShellHost process and remembers its state on subsequent openings.
- **Microphone volume** defaults to **Off**. **Auto** adds a volume slider above system volume when the selected microphone has an adjustable level. The button on the left mutes and unmutes the microphone without changing its level.
- **Microphone settings shortcut** shows the volume percentage by default. **Device settings** replaces it with a button for the selected microphone's Windows settings.
- **Sound settings shortcut** puts the Windows Sound settings button beside system volume by default. **None (Percentage)** shows the volume percentage. **Device settings** opens the selected output device's settings. Both modes move the Sound settings button to the header.
- **Spatial sound** controls whether the spatial audio menu appears. It is on by default.
- **Sound field shape** rounds the closed output, input and spatial sound selectors in **Pills** mode.
- Volume sliders show the new value when dragged or scrolled. The default wheel step is 2 percentage points.
- Press a volume number to set that slider to 100. The app icon mutes or restores the app.
- **Hide from the volume mixer** takes app names or executable names, one per row. Hidden apps keep playing audio.

**Taskbar**

- **Quick access to Sound** opens Sound with a click on the taskbar volume icon.
- **Middle-click to mute** toggles sound from the volume icon and preserves the volume level. Off by default.
- **More sound settings** in the volume icon menu opens the classic Sound Control Panel.
- **Device Manager** is available in the network and battery icon menus.
- **Alternative icons** includes the animated volume and wired network icons from the original mod. Each location has its own switch. Switching one off uses the Windows icon there.

The interface is available in English, Russian, Spanish, German, Brazilian Portuguese and Simplified Chinese.

## Compatibility

Requires Windows 11 24H2 or later (x64).

Disable other mods or styling rules that modify the same Quick Settings, Sound or taskbar volume elements.

Control Center discovery uses COM-interface symbol hooks in ControlCenter.dll and the public XAML visual tree; it does not open a XAML Diagnostics connection. Compatibility with other mods that edit the same controls still requires live testing. If the panel was already open when enabling the mod, close and reopen it; restart ShellHost if it is not picked up.

## Installation from source

1. Install [Windhawk](https://windhawk.net/).
2. Download [the standalone mod source](mod/dist/refined-quick-settings-and-sound.wh.cpp).
3. In Windhawk, create a new mod, replace the template with the downloaded source, then compile and enable it.
4. If the panels do not update, restart ShellHost. Windhawk logs contain diagnostic errors.

No prebuilt DLL is required. Open Quick Settings with Win+A and Sound with Win+Ctrl+V. The taskbar volume icon also opens Sound when **Quick access to Sound** is enabled.

The repeatable mouse demo recorder is documented in [docs/PREVIEW.ru.md](docs/PREVIEW.ru.md).

## Building

Requires Python 3 and an installed Windhawk (`build.ps1` uses `C:/Program Files/Windhawk/Compiler` and Engine 1.7.3; adjust the paths for another installation).

```powershell
cd mod
.\build.ps1
.\test-model.ps1
```

`build.ps1` first runs `package.py`, which assembles `mod/dist/refined-quick-settings-and-sound.wh.cpp`, a single file for the Windhawk editor. Edit the sources in `mod/`, not `dist`. `mod/test-model.ps1` checks the model logic and property interfaces. Panel opening timing: `tools/measure-panel-open.ps1`.

## Licenses

The mod is [GPL-3.0-only](LICENSE). The UI-thread dispatch technique follows m417z's Notification Center Styler (GPL-3.0); the hidden-apps parser is adapted from the Sound Output Direct and Taller mod. Lucide icons are ISC licensed, see [third-party notices](docs/THIRD-PARTY-NOTICES.md) and `mod/LUCIDE-LICENSE.txt`.

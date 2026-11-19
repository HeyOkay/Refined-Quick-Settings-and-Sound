# Refined Quick Settings and Sound

[Русский](docs/README.ru.md)

![Dark theme](docs/assets/preview-lossless.webp?v=01bbd42)

![Light theme](docs/assets/preview-light-lossless.webp)

This "vanilla+" Windhawk mod reworks the Windows 11 "Quick Settings" and "Volume mixer" panels and makes them fit for daily use. It follows the Windows 11 design language with thin sliders, even row spacing and a smooth frame transition to the Sound page.

Everything runs inside the shell's own panels (ShellHost). The system Acrylic, fonts, themes, lists, bindings and animations are kept, and the original layout is restored when the mod is unloaded.

## Features

**Quick Settings**

- Choose up to five **Additional shortcuts** to the right of **Settings**, including Notifications, Graphics, Advanced network settings, Power and Windows Update. Empty slots and duplicate shortcuts are skipped. The Display shortcut is also skipped when the brightness row already has a Display button.
- The brightness row places the native Windows slider, or the DDC/CI slider, between Night light and Display settings. Night light uses the native quick action when available and opens its settings otherwise.
- **Display brightness** defaults to **Auto**. It keeps the Windows slider when available or adds one for a monitor with a usable DDC/CI range. **Disable** hides the brightness row.
- Choose the default or pill shape independently for Quick Settings tiles and closed Sound selection fields under **Appearance**.
- Set **Rows of tiles** to 1–4. Pages follow the number of available tiles. The override requires the ControlCenter build from Windows 26300.9974.

**Sound**

- The "Sound output" page is now simply "Sound".
- Change the output and input devices and the spatial sound provider from dropdown lists.
- Below them is a reworked Volume mixer for the apps. It starts collapsed the first time Sound opens in each ShellHost process and remembers its state on subsequent openings.
- Volume sliders show the new value when dragged or scrolled. The default wheel step is 2 percentage points.
- Press an app's icon to mute and restore it, or its volume number to set it to 100.
- **Hide from the volume mixer** takes app names or executable names, one per row. Hidden apps keep playing audio.

**Taskbar**

- **Quick access to Sound** opens Sound with a click on the taskbar volume icon.
- **Middle-click to mute** toggles sound from the volume icon and preserves the volume level. Off by default.
- **More sound settings** in the volume icon menu opens the classic Sound Control Panel.
- **Device Manager** is available in the network and battery icon menus.
- **Alternative icons** includes the animated volume and wired network icons from the original mod. Each location has its own switch. Switching one off uses the Windows icon there.

## Compatibility

Windows 11 24H2 or later (Quick Settings hosted by ShellHost), x86-64 (ARM64 is untested). Development testing was performed on build **26300**, primarily at **125%** display scale. Other builds and scales need validation; this mod depends on internal shell XAML elements and symbols that Windows updates can change. Added labels are available in English, Russian, Spanish, German, Brazilian Portuguese and Simplified Chinese. DDC/CI brightness requires a compatible monitor with DDC/CI enabled in its settings.

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

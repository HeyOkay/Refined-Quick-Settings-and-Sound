// ==WindhawkMod==
// @id refined-quick-settings-and-sound
// @name Refined Quick Settings and Sound
// @description A "vanilla+" rework of the Windows 11 Quick Settings and Volume mixer panels for daily use
// @version 0.1.154
// @author HeyOkay
// @github https://github.com/HeyOkay
// @include explorer.exe
// @include ShellHost.exe
// @architecture x86-64
// @compilerOptions -DWIN32_LEAN_AND_MEAN -lcomctl32 -lole32 -loleaut32 -lruntimeobject -liphlpapi -lshlwapi -lshell32 -lpowrprof -luuid
// @license GPL-3.0-only
// ==/WindhawkMod==
// ==WindhawkModReadme==
/*
# Refined Quick Settings and Sound

![Quick Settings and Sound preview](https://raw.githubusercontent.com/HeyOkay/Refined-Quick-Settings-and-Sound/main/docs/assets/preview-lossless.webp)

This "vanilla+" Windhawk mod reworks the Windows 11 "Quick Settings" and
"Volume mixer" panels and makes them fit for daily use. It follows the
Windows 11 design language, refining existing features and adding new ones:
thin sliders, even row spacing at every display scale and a smooth frame
transition to the Sound page.

Everything runs inside the shell's own panels (ShellHost): no extra windows,
the system Acrylic, fonts, themes, lists, bindings and animations are kept, and
customizations are restored when the mod is unloaded.

## Features

**Quick Settings**

- Brightness control for DDC/CI-compatible displays, with animated glyphs.
- Press the brightness glyph to toggle night light on and off.
- Open display settings directly from the glyph on the right side of the slider.

**Sound**

- The "Sound output" page is now simply "Sound".
- Change the output and input devices and the spatial sound provider from
  dropdown lists.
- Below them, a reworked Volume mixer for the apps. It is collapsible and
  remembers its last state.
- Mouse wheel on every slider; press an app's icon to mute and restore it, or
  its volume number to set it to 100.
- Hide apps from the mixer by name (setting below).

**Taskbar**

- Click the volume icon to open Sound directly.
- The system's animated volume glyph and an Ethernet glyph for wired networks.

## Compatibility

- Windows 11 24H2 or later (Quick Settings hosted by ShellHost), x86-64
  (ARM64 is untested).
  Development testing: build 26300, primarily at 125% scale.
- Other Windows builds and display scales need validation. The mod depends on
  internal shell XAML elements and symbols which can change with Windows updates.
- DDC/CI brightness requires a compatible monitor with DDC/CI enabled.
- Added labels such as "Sound" and "Input device" are in English.
- The first Sound opening in each ShellHost process starts with the mixer
  collapsed; subsequent openings remember its state.

Control Center discovery uses COM-interface symbol hooks in ControlCenter.dll and the public XAML visual tree; it does not open a XAML Diagnostics connection. Compatibility with other mods that edit the same controls still requires live testing. If the panel was already open when enabling the mod, close and reopen it; restart ShellHost if it is not picked up.

## Notes

Disable other mods that change the Quick Settings, the Sound page or the
taskbar volume icon before enabling this one. Errors are written to the
Windhawk log.

## Credits and license

GPL-3.0-only. The UI-thread dispatch technique follows
[m417z's Notification Center Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-notification-center-styler.wh.cpp)
(GPL-3.0). ControlCenter.dll discovery uses the COM produce-interface pattern
documented by [Per-monitor brightness](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/per-monitor-brightness.wh.cpp).
The hidden-applications parser is adapted from the local
Sound Output Direct and Taller mod (GPL-3.0-only).
The sliders-horizontal and ethernet-port icons are from
[Lucide](https://lucide.dev/) (ISC); the license notice is included below
in this source file.
*/
// ==/WindhawkModReadme==
// ==WindhawkModSettings==
/*
- hiddenApps: ""
  $name: Hidden applications
  $description: Parts of application names, separated by semicolons, to hide from the volume mixer. Hiding does not mute them.
*/
// ==/WindhawkModSettings==
#include "host.h"
#include "tray.h"
namespace native {
inline bool explorer = false;
inline void Settings() {
  auto setting = WindhawkUtils::StringSetting::make(L"hiddenApps");
  hidden = sound::ParseHidden(setting.get());
}
} // namespace native
BOOL Wh_ModInit() {
  using namespace native;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&Wh_ModInit), &module);
  WCHAR path[32768]{};
  GetModuleFileNameW(nullptr, path, 32768);
  explorer = !_wcsicmp(PathFindFileNameW(path), L"explorer.exe");
  stopping = false;
  Settings();
  if (explorer) {
    if (!tray::Initialize(module)) {
      tray::Shutdown();
      return FALSE;
    }
    return TRUE;
  }
  return InitializeHost();
}
void Wh_ModAfterInit() {
  if (native::explorer) {
    native::tray::AttachExisting();
  } else
    native::AfterHostInit();
}
void Wh_ModUninit() {
  native::stopping = true;
  if (native::explorer) {
    native::tray::Shutdown();
  } else
    native::StopHost();
}
BOOL Wh_ModSettingsChanged(BOOL *reload) {
  *reload = TRUE;
  return TRUE;
}

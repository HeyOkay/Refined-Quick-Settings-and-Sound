#pragma once
#include "common.h"

namespace native {
inline ModSettings ReadPanelSettings(auto readInt, auto readString) {
  ModSettings settings;
  settings.quickSoundAccess = readInt(L"taskbar.openSoundFromVolumeIcon") != 0;
  settings.trayMiddleClickMute = readInt(L"taskbar.middleClickMute") != 0;
  settings.volumeWheelStep = sound::VolumeWheelStep(readInt(L"sound.volumeWheelStep"));
  settings.brightnessEnabled = readString(L"quickSettings.brightnessMode") != L"disabled";
  settings.pillQuickTiles = readString(L"quickSettings.tileShape") == L"pills";
  settings.pillSoundPickers = readString(L"sound.selectorShape") == L"pills";
  settings.sound = sound::ParseSoundOptions(readString(L"sound.microphoneVolumeMode"),
                                           readString(L"sound.microphoneVolumeRight"),
                                           readString(L"sound.systemVolumeRight"),
                                           readString(L"sound.spatialSoundMenu"));

  auto rows = readString(L"quickSettings.tileRows");
  if (rows.size() == 1 && rows.front() >= L'1' && rows.front() <= L'4')
    settings.quickActionRows = rows.front() - L'0';

  for (size_t i = 0; i < settings.quickShortcuts.size(); ++i) {
    auto key = L"quickSettings.additionalShortcuts.shortcut" + std::to_wstring(i + 1);
    settings.quickShortcuts[i] = sound::ParseQuickShortcut(readString(key.c_str()));
  }

  // Windhawk exposes individual slots without reporting the array length.
  for (int i = 0, blanks = 0; blanks < 32; ++i) {
    auto key = L"sound.hiddenApplications[" + std::to_wstring(i) + L"]";
    auto value = readString(key.c_str());
    if (value.empty()) {
      ++blanks;
      continue;
    }
    blanks = 0;
    sound::AddHiddenApp(settings.hidden, value);
  }
  ReadAlternativeIcons(settings, readInt);
  return settings;
}
} // namespace native

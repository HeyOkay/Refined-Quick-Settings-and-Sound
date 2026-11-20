#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>
namespace sound {
inline PCWSTR Text(PCWSTR english, PCWSTR russian, PCWSTR spanish, PCWSTR german,
                   PCWSTR portuguese, PCWSTR chinese) {
  switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
  case LANG_RUSSIAN: return russian;
  case LANG_SPANISH: return spanish;
  case LANG_GERMAN: return german;
  case LANG_PORTUGUESE: return portuguese;
  case LANG_CHINESE: return chinese;
  default: return english;
  }
}

enum class QuickShortcut {
  None, Display, Network, Bluetooth, Apps, Notifications, Graphics, NetworkSettings, Power, Update
};
using QuickShortcutSlots = std::array<QuickShortcut, 5>;
inline QuickShortcut ParseQuickShortcut(std::wstring_view value) {
  if (value == L"display") return QuickShortcut::Display;
  if (value == L"network") return QuickShortcut::Network;
  if (value == L"bluetooth") return QuickShortcut::Bluetooth;
  if (value == L"apps") return QuickShortcut::Apps;
  if (value == L"notifications") return QuickShortcut::Notifications;
  if (value == L"graphics") return QuickShortcut::Graphics;
  if (value == L"networkSettings") return QuickShortcut::NetworkSettings;
  if (value == L"power") return QuickShortcut::Power;
  if (value == L"update") return QuickShortcut::Update;
  return QuickShortcut::None;
}
inline std::vector<QuickShortcut> QuickShortcuts(QuickShortcutSlots const &slots,
                                                bool displayBelow) {
  std::vector<QuickShortcut> result;
  for (auto action : slots)
    if (action != QuickShortcut::None &&
        !(action == QuickShortcut::Display && displayBelow) &&
        std::find(result.begin(), result.end(), action) == result.end())
      result.push_back(action);
  return result;
}

// Adapted from the hidden-applications parser of the Sound Output Direct and
// Taller Windhawk mod; the only code taken from it.
// Matching happens before a row is created, so hidden apps occupy no space.
inline std::wstring Lower(std::wstring value) {
  if (!value.empty())
    CharLowerBuffW(value.data(), static_cast<DWORD>(value.size()));
  return value;
}
inline void AddHiddenApp(std::vector<std::wstring> &apps, std::wstring_view name) {
  auto first = name.find_first_not_of(L" \t\r\n");
  if (first != name.npos)
    apps.push_back(Lower(std::wstring(name.substr(first, name.find_last_not_of(L" \t\r\n") - first + 1))));
}
inline int VolumeWheelStep(int value) {
  return value >= 1 && value <= 100 ? value : 2;
}
struct WheelVolume {
  int64_t remainder = 0;
  double Next(double current, double minimum, double maximum, int delta, int step) {
    auto movement = remainder + int64_t(delta) * step;
    auto target = current + double(movement / WHEEL_DELTA);
    remainder = movement % WHEEL_DELTA;
    if ((target <= minimum && remainder < 0) || (target >= maximum && remainder > 0))
      remainder = 0;
    return std::clamp(target, minimum, maximum);
  }
};
inline bool Hidden(std::vector<std::wstring> const &patterns, std::wstring const &name,
                   std::wstring const &path) {
  auto text = Lower(name + L" " + path);
  return std::any_of(patterns.begin(), patterns.end(),
                     [&](auto const &p) { return text.find(p) != text.npos; });
}
inline float Clamp(float value) { return std::clamp(value, 0.f, 1.f); }
inline double EndpointVolumePercent(float value, bool muted) {
  return muted ? 0 : std::round(Clamp(value) * 100);
}
inline int VolumeIcon(float value, bool mute) {
  return mute ? -1 : value <= 0 ? 0 : value <= .33f ? 1 : value <= .66f ? 2 : 3;
}
struct SavedVolume {
  float last = .5f;
  void Observe(float value, bool) {
    if (value > 0)
      last = Clamp(value);
  }
  float Toggle(float current, bool muted) {
    Observe(current, muted);
    return !muted && current > 0 ? 0 : last;
  }
};
struct Device {
  std::wstring id, name;
  bool current = false;
};
struct Snapshot {
  uint64_t masterNoticeAt = 0, masterRevision = 0;
  std::vector<Device> output, input;
  std::wstring outputId, inputId, error;
  float master = 0;
  bool muted = false, masterAvailable = false;
  float inputVolume = 0;
  bool inputVolumeAvailable = false, inputMuteAvailable = false, inputMuted = false;
  unsigned long long revision = 0;
  unsigned long long applied = 0;
};
enum class InputVolumeProbe { Idle, Poll, Failed };
inline bool UnsupportedInputVolume(HRESULT result) {
  return result == E_NOINTERFACE || result == E_NOTIMPL ||
         result == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}
class InputVolumeRetry {
  unsigned failures_ = 0;

public:
  std::chrono::seconds Delay(InputVolumeProbe probe) {
    if (probe == InputVolumeProbe::Failed) {
      auto delay = std::chrono::seconds{1 << failures_};
      failures_ = std::min(failures_ + 1, 3u);
      return delay;
    }
    failures_ = 0;
    return std::chrono::seconds{probe == InputVolumeProbe::Poll ? 1 : 0};
  }
};
enum class SystemVolumeRight { SoundSettings, Percentage, DeviceSettings };
struct SoundOptions {
  bool microphoneAuto = false, microphoneDeviceSettings = false, spatialSound = true;
  SystemVolumeRight systemVolumeRight = SystemVolumeRight::SoundSettings;
  bool SettingsInHeader() const { return systemVolumeRight != SystemVolumeRight::SoundSettings; }
  bool operator==(SoundOptions const &) const = default;
};
inline SoundOptions ParseSoundOptions(std::wstring_view microphone, std::wstring_view microphoneRight,
                                      std::wstring_view systemRight, std::wstring_view spatial) {
  SoundOptions options;
  options.microphoneAuto = microphone == L"auto";
  options.microphoneDeviceSettings = microphoneRight == L"deviceSettings";
  options.spatialSound = spatial != L"disabled";
  if (systemRight == L"percentage")
    options.systemVolumeRight = SystemVolumeRight::Percentage;
  else if (systemRight == L"deviceSettings")
    options.systemVolumeRight = SystemVolumeRight::DeviceSettings;
  return options;
}
inline bool ShowMicrophoneRow(SoundOptions options, Snapshot const &state) {
  return options.microphoneAuto && !state.inputId.empty() && state.inputVolumeAvailable;
}
inline bool CanResetVolume(double value, bool showMuteAtZero = false) {
  return value >= 0 && value < 100 && (value > 0 || !showMuteAtZero);
}
inline std::wstring AudioDevicePropertiesUri(std::wstring_view id) {
  if (id.empty())
    return {};
  return std::wstring(L"ms-settings:sound-properties?endpointId=").append(id);
}
enum class Action {
  MasterVolume, MasterMute, ToggleEndpointMute, Input, InputVolume, InputMute, Refresh
};
struct Command {
  Action action = Action::Refresh;
  std::wstring id;
  float value = 0;
  unsigned long long serial = 0;
};
} // namespace sound

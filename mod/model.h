#pragma once
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <map>
#include <string>
#include <vector>
#include <windows.h>
namespace sound {
// Adapted from the hidden-applications parser of the Sound Output Direct and
// Taller Windhawk mod; the only code taken from it.
// Matching happens before a row is created, so hidden apps occupy no space.
inline std::wstring Lower(std::wstring value) {
  if (!value.empty())
    CharLowerBuffW(value.data(), static_cast<DWORD>(value.size()));
  return value;
}
inline std::vector<std::wstring> ParseHidden(std::wstring const &text) {
  std::vector<std::wstring> result;
  size_t start = 0;
  while (start < text.size()) {
    auto end = text.find_first_of(L";,\r\n", start);
    if (end == text.npos)
      end = text.size();
    auto part = text.substr(start, end - start);
    auto first = part.find_first_not_of(L" \t");
    if (first != part.npos)
      result.push_back(Lower(part.substr(first, part.find_last_not_of(L" \t") - first + 1)));
    start = end + 1;
  }
  return result;
}
inline bool Hidden(std::vector<std::wstring> const &patterns, std::wstring const &name,
                   std::wstring const &path) {
  auto text = Lower(name + L" " + path);
  return std::any_of(patterns.begin(), patterns.end(),
                     [&](auto const &p) { return text.find(p) != text.npos; });
}
inline float Clamp(float value) { return std::clamp(value, 0.f, 1.f); }
inline int VolumeIcon(float value, bool mute) {
  return mute || value <= 0 ? 0 : value <= .33f ? 1 : value <= .66f ? 2 : 3;
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
struct App {
  std::wstring id, name, path, icon;
  float volume = 0;
  bool muted = false;
};
struct Spatial {
  std::wstring id, name;
  bool current = false;
};
struct Snapshot {
  uint64_t masterNoticeAt = 0, masterRevision = 0;
  std::vector<Device> output, input;
  std::vector<App> apps;
  std::vector<Spatial> spatial;
  std::wstring outputId, inputId, error;
  float master = 0;
  bool muted = false, masterAvailable = false, spatialAvailable = false;
  unsigned long long revision = 0;
  unsigned long long applied = 0;
};
enum class Action { MasterVolume, MasterMute, AppVolume, AppMute, Output, Input, Spatial, Refresh };
struct Command {
  Action action = Action::Refresh;
  std::wstring id;
  float value = 0;
  unsigned long long serial = 0;
};
} // namespace sound

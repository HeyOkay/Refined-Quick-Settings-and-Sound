#define WH_EDITING
#define WH_MOD
#define WH_MOD_ID L"native-model-test"
#include <windhawk_api.h>
#include "quick.h"
#include "settings.h"
#include <cstdio>
namespace F = winrt::Windows::Foundation;
struct Store {
  F::PropertyType kind;
  double number = 37;
  F::PropertyType written = F::PropertyType::Empty;
};
struct Property : winrt::implements<Property, native::D::ICustomProperty> {
  std::shared_ptr<Store> store;
  explicit Property(std::shared_ptr<Store> value) : store(std::move(value)) {}
  auto Type() { return winrt::xaml_typename<double>(); }
  winrt::hstring Name() { return L"Volume"; }
  bool CanRead() { return true; }
  bool CanWrite() { return true; }
  native::IInspectable GetValue(native::IInspectable const &) {
    switch (store->kind) {
    case F::PropertyType::Int32:
      return winrt::box_value(int32_t(store->number));
    case F::PropertyType::UInt32:
      return winrt::box_value(uint32_t(store->number));
    case F::PropertyType::Single:
      return winrt::box_value(float(store->number));
    default:
      return winrt::box_value(store->number);
    }
  }
  void SetValue(native::IInspectable const &, native::IInspectable const &value) {
    auto number = value.as<F::IPropertyValue>();
    store->written = number.Type();
    switch (number.Type()) {
    case F::PropertyType::Int32:
      store->number = number.GetInt32();
      break;
    case F::PropertyType::UInt32:
      store->number = number.GetUInt32();
      break;
    case F::PropertyType::Single:
      store->number = number.GetSingle();
      break;
    case F::PropertyType::Double:
      store->number = number.GetDouble();
      break;
    default:
      throw winrt::hresult_invalid_argument();
    }
  }
  native::IInspectable GetIndexedValue(native::IInspectable const &, native::IInspectable const &) {
    return nullptr;
  }
  void SetIndexedValue(native::IInspectable const &, native::IInspectable const &,
                       native::IInspectable const &) {}
};
struct Model : winrt::implements<Model, native::D::ICustomPropertyProvider> {
  std::shared_ptr<Store> store;
  explicit Model(std::shared_ptr<Store> value) : store(std::move(value)) {}
  native::D::ICustomProperty GetCustomProperty(winrt::hstring const &name) {
    return name == L"Volume" ? winrt::make<Property>(store) : nullptr;
  }
  native::D::ICustomProperty GetIndexedProperty(winrt::hstring const &,
                                                native::X::Interop::TypeName const &) {
    return nullptr;
  }
  winrt::hstring GetStringRepresentation() { return L"Native volume test"; }
  native::X::Interop::TypeName Type() {
    return {L"NativeVolumeTest", native::X::Interop::TypeKind::Custom};
  }
};
int main() {
  try {
    using Right = sound::SystemVolumeRight;
    auto defaults = sound::ParseSoundOptions(L"", L"", L"", L"");
    auto customized = sound::ParseSoundOptions(L"auto", L"deviceSettings", L"deviceSettings", L"disabled");
    auto percentages = sound::ParseSoundOptions(L"auto", L"percentage", L"percentage", L"enabled");
    auto system = sound::ParseSoundOptions(L"disabled", L"other", L"soundSettings", L"enabled");
    sound::Snapshot input;
    if (defaults.microphoneAuto || defaults.microphoneDeviceSettings || !defaults.spatialSound ||
        defaults.systemVolumeRight != Right::SoundSettings || defaults.SettingsInHeader() ||
        !customized.microphoneAuto || !customized.microphoneDeviceSettings || customized.spatialSound ||
        customized.systemVolumeRight != Right::DeviceSettings || !customized.SettingsInHeader() ||
        percentages.microphoneDeviceSettings || percentages.systemVolumeRight != Right::Percentage ||
        !percentages.SettingsInHeader() || system.microphoneDeviceSettings || system.SettingsInHeader() ||
        sound::ShowMicrophoneRow(customized, input)) {
      std::puts("FAIL: Volume rows must use the selected trailing controls and show one Sound settings button");
      return 13;
    }
    input.inputId = L"{0.0.1.00000000}.{aaaaaaaa-0000-1111-2222-bbbbbbbbbbbb}";
    if (sound::ShowMicrophoneRow(customized, input)) {
      std::puts("FAIL: A microphone without a usable level control gets a slider");
      return 13;
    }
    input.inputVolumeAvailable = true;
    if (!sound::ShowMicrophoneRow(customized, input) ||
        sound::ShowMicrophoneRow(defaults, input) ||
        !sound::AudioDevicePropertiesUri(L"").empty() ||
        sound::AudioDevicePropertiesUri(input.inputId) !=
            L"ms-settings:sound-properties?endpointId={0.0.1.00000000}.{aaaaaaaa-0000-1111-2222-bbbbbbbbbbbb}") {
      std::puts("FAIL: The input row and its link must target the available selected microphone");
      return 13;
    }
    std::puts("PASS: Sound options preserve defaults, gate the microphone row and link to the selected endpoint");
    input.inputVolume = .64f;
    input.inputMuted = true;
    if (sound::EndpointVolumePercent(input.inputVolume, input.inputMuted) != 0 ||
        input.inputVolume != .64f) {
      std::puts("FAIL: a muted endpoint must display zero and retain its stored level");
      return 14;
    }
    input.inputMuted = false;
    if (sound::EndpointVolumePercent(input.inputVolume, input.inputMuted) != 64) {
      std::puts("FAIL: unmuting must show the stored endpoint level");
      return 14;
    }
    std::puts("PASS: muting displays zero and unmuting restores the displayed endpoint level");
    if (!sound::CanResetVolume(0) || !sound::CanResetVolume(57) ||
        sound::CanResetVolume(100) || sound::CanResetVolume(NAN) ||
        sound::CanResetVolume(0, true) || !sound::CanResetVolume(57, true)) {
      std::puts("FAIL: A visible numeric volume below 100 must reset, including zero when shown");
      return 14;
    }
    std::puts("PASS: A visible numeric volume can be reset from zero or a partial level");
    {
      // Old values that Windhawk may still store cannot override grouped settings.
      std::map<std::wstring, int> numbers{{L"taskbar.openSoundFromVolumeIcon", 1},
                                         {L"taskbar.middleClickMute", 0},
                                         {L"sound.volumeWheelStep", 2},
                                         {L"quickSoundAccess", 0},
                                         {L"volumeWheelStep", 7}};
      std::map<std::wstring, std::wstring> strings{
          {L"quickSettings.brightnessMode", L"auto"}, {L"brightnessMode", L"disabled"},
          {L"quickSettings.tileRows", L"2"}, {L"quickSettings.tileShape", L"default"},
          {L"sound.selectorShape", L"default"}, {L"sound.microphoneVolumeMode", L"disabled"},
          {L"sound.spatialSoundMenu", L"enabled"}, {L"sound.systemVolumeRight", L"soundSettings"},
          {L"quickSettings.additionalShortcuts.shortcut1", L"display"},
          {L"quickSettings.shortcut1", L"none"}, {L"hiddenApplications[0]", L"Old.exe"}};
      auto readInt = [&](PCWSTR key) { return numbers[key]; };
      auto readString = [&](PCWSTR key) { return strings[key]; };
      auto settings = native::ReadPanelSettings(readInt, readString);
      using Shortcut = sound::QuickShortcut;
      if (!settings.quickSoundAccess || settings.trayMiddleClickMute || settings.volumeWheelStep != 2 ||
          !settings.brightnessEnabled || settings.quickActionRows != 2 || settings.pillQuickTiles ||
          settings.pillSoundPickers || settings.sound.microphoneAuto || !settings.sound.spatialSound ||
          settings.sound.systemVolumeRight != Right::SoundSettings ||
          settings.quickShortcuts[0] != Shortcut::Display || !settings.hidden.empty()) {
        std::puts("FAIL: Old values overrode the grouped defaults");
        return 16;
      }
      numbers[L"taskbar.openSoundFromVolumeIcon"] = 0;
      numbers[L"taskbar.middleClickMute"] = 1;
      numbers[L"sound.volumeWheelStep"] = 11;
      strings[L"quickSettings.brightnessMode"] = L"disabled";
      strings[L"quickSettings.tileRows"] = L"4";
      strings[L"quickSettings.tileShape"] = L"pills";
      strings[L"sound.selectorShape"] = L"pills";
      strings[L"sound.microphoneVolumeMode"] = L"auto";
      strings[L"sound.spatialSoundMenu"] = L"disabled";
      strings[L"sound.systemVolumeRight"] = L"percentage";
      strings[L"quickSettings.additionalShortcuts.shortcut4"] = L"graphics";
      strings[L"sound.hiddenApplications[0]"] = L"New.exe";
      strings[L"sound.hiddenApplications[32]"] = L"Final.exe";
      strings[L"sound.hiddenApplications[65]"] = L"BeyondGap.exe";
      settings = native::ReadPanelSettings(readInt, readString);
      if (settings.quickSoundAccess || !settings.trayMiddleClickMute || settings.volumeWheelStep != 11 ||
          settings.brightnessEnabled || settings.quickActionRows != 4 || !settings.pillQuickTiles ||
          !settings.pillSoundPickers || !settings.sound.microphoneAuto || settings.sound.spatialSound ||
          settings.sound.systemVolumeRight != Right::Percentage || settings.quickShortcuts[3] != Shortcut::Graphics ||
          settings.hidden != std::vector<std::wstring>{L"new.exe", L"final.exe"}) {
        std::puts("FAIL: Grouped preferences or sparse hidden-app rows were not applied");
        return 16;
      }
      for (int i = 0; i < 33; ++i) {
        auto index = std::to_wstring(i);
        strings[L"sound.hiddenApplications[" + index + L"]"] = L"App" + index + L".exe";
      }
      settings = native::ReadPanelSettings(readInt, readString);
      if (settings.hidden.size() != 33 || settings.hidden.back() != L"app32.exe") {
        std::puts("FAIL: a full hidden-app list was truncated at 32 entries");
        return 16;
      }
      std::puts("PASS: grouped preferences ignore old keys and preserve long and sparse hidden-app lists");
    }
    {
      using Probe = sound::InputVolumeProbe;
      sound::InputVolumeRetry retry;
      size_t attempt = 0;
      for (int delay : {1, 2, 4, 8, 8}) {
        if (retry.Delay(Probe::Failed).count() != delay) {
          std::printf("FAIL: microphone retry %zu had the wrong delay\n", ++attempt);
          return 15;
        }
        ++attempt;
      }
      if (retry.Delay(Probe::Idle).count() != 0 || retry.Delay(Probe::Failed).count() != 1 ||
          retry.Delay(Probe::Poll).count() != 1 || retry.Delay(Probe::Failed).count() != 1 ||
          !sound::UnsupportedInputVolume(E_NOTIMPL) || !sound::UnsupportedInputVolume(E_NOINTERFACE) ||
          !sound::UnsupportedInputVolume(HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)) ||
          sound::UnsupportedInputVolume(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED))) {
        std::puts("FAIL: microphone retry must reset on recovery and distinguish unsupported endpoints");
        return 15;
      }
      std::puts("PASS: microphone failures back off and stop polling after recovery or unsupported results");
    }
    auto soundRequest = native::SoundRequestToken(0xfffffff0, 1);
    if (soundRequest == native::SoundRequestToken(0xfffffff0, 2) ||
        native::SoundEventMatchesRequest(soundRequest, 0xffffffef) ||
        !native::SoundEventMatchesRequest(soundRequest, 0xfffffff0) ||
        !native::SoundEventMatchesRequest(soundRequest, 1)) {
      std::puts("FAIL: Sound request ignores earlier shell events across tick wraparound");
      return 9;
    }
    std::puts("PASS: Sound request tokens distinguish taps and reject stale shell events");
    native::originalSetQuickRows = [](void *grid, int rows) { *static_cast<int *>(grid) = rows; };
    native::updateQuickExtent = [](void *) {};
    native::modSettings.quickActionRows = 1;
    int nativeGrid = 0;
    native::KeepQuickRows(&nativeGrid, 2);
    if (nativeGrid != 2) {
      std::puts("FAIL: Quick Settings rows were overridden before the grid was tracked");
      return 10;
    }
    int secondGrid = 2;
    if (!native::TrackQuickRows(&nativeGrid, nativeGrid, 1) ||
        !native::TrackQuickRows(&secondGrid, secondGrid, 3)) {
      std::puts("FAIL: Quick Settings grids did not acquire separate overrides");
      return 10;
    }
    native::KeepQuickRows(&nativeGrid, 2);
    native::KeepQuickRows(&secondGrid, 2);
    if (nativeGrid != 1 || secondGrid != 3) {
      std::puts("FAIL: Quick Settings grids did not apply their selected row counts");
      return 10;
    }
    native::KeepQuickRows(&secondGrid, 1);
    native::RestoreQuickRows(&nativeGrid);
    native::RestoreQuickRows(&secondGrid);
    if (nativeGrid != 2 || secondGrid != 1) {
      std::puts("FAIL: Quick Settings grids did not restore the latest native row counts");
      return 10;
    }
    native::KeepQuickRows(&secondGrid, 2);
    if (secondGrid != 2) {
      std::puts("FAIL: Quick Settings still overrides rows after restoration");
      return 10;
    }
    std::puts("PASS: Quick Settings preserves and restores the native row count per grid");
    using Shortcut = sound::QuickShortcut;
    auto buttons = sound::QuickShortcuts(
        {Shortcut::Display, Shortcut::None, Shortcut::Display, Shortcut::Power, Shortcut::Power},
        false);
    auto withoutDisplay = sound::QuickShortcuts(
        {Shortcut::Display, Shortcut::Network, Shortcut::NetworkSettings, Shortcut::None,
         Shortcut::Bluetooth}, true);
    auto empty = sound::QuickShortcuts(
        {Shortcut::Display, Shortcut::Display, sound::ParseQuickShortcut(L"night")}, true);
    auto added = sound::QuickShortcuts(
        {sound::ParseQuickShortcut(L"notifications"), sound::ParseQuickShortcut(L"graphics"),
         sound::ParseQuickShortcut(L"networkSettings"), sound::ParseQuickShortcut(L"power"),
         sound::ParseQuickShortcut(L"update")}, false);
    if (buttons != std::vector{Shortcut::Display, Shortcut::Power} ||
        withoutDisplay != std::vector{Shortcut::Network, Shortcut::NetworkSettings,
                                      Shortcut::Bluetooth} ||
        !empty.empty() ||
        added != std::vector{Shortcut::Notifications, Shortcut::Graphics, Shortcut::NetworkSettings,
                             Shortcut::Power, Shortcut::Update} ||
        sound::ParseQuickShortcut(L"apps") != Shortcut::Apps ||
        sound::ParseQuickShortcut(L"unknown") != Shortcut::None) {
      std::puts("FAIL: Quick Settings shortcut order and visibility");
      return 5;
    }
    std::puts("PASS: Quick Settings shortcuts skip gaps, duplicates and visible slider commands");
    sound::WheelVolume wheel;
    if (sound::VolumeWheelStep(0) != 2 || sound::VolumeWheelStep(7) != 7 ||
        wheel.Next(50, 0, 100, 120, 2) != 52 ||
        wheel.Next(99, 0, 100, 240, 7) != 100 ||
        wheel.Next(50, 0, 100, 30, 2) != 50 ||
        wheel.Next(50, 0, 100, 30, 2) != 51 ||
        wheel.Next(51, 0, 100, -120, 2) != 49) {
      std::puts("FAIL: Volume wheel defaults, bounds and partial detents");
      return 6;
    }
    std::vector<std::wstring> names;
    sound::AddHiddenApp(names, L"  Foo, Inc. ");
    sound::AddHiddenApp(names, L" ");
    sound::AddHiddenApp(names, L"STEAM.exe");
    if (names != std::vector<std::wstring>{L"foo, inc.", L"steam.exe"}) {
      std::puts("FAIL: Hidden app rows must keep punctuation and skip blanks");
      return 7;
    }
    std::puts("PASS: Volume wheel and hidden app setting behavior");
    using IconSwitch = bool native::ModSettings::*;
    const std::pair<std::wstring_view, IconSwitch> iconSwitches[] = {
        {L"alternativeIcons.mixer.quick", &native::ModSettings::altQuickMixer},
        {L"alternativeIcons.mixer.sound", &native::ModSettings::altSoundMixer},
        {L"alternativeIcons.volume.taskbar", &native::ModSettings::altTrayVolume},
        {L"alternativeIcons.volume.quick", &native::ModSettings::altQuickVolume},
        {L"alternativeIcons.volume.sound", &native::ModSettings::altSoundVolume},
        {L"alternativeIcons.volume.apps", &native::ModSettings::altAppUnmute},
        {L"alternativeIcons.ethernet.taskbar", &native::ModSettings::altTrayEthernet},
        {L"alternativeIcons.gear.sound", &native::ModSettings::altSoundGear},
    };
    native::ModSettings icons;
    native::ReadAlternativeIcons(icons, [](PCWSTR) { return 1; });
    if (icons != native::ModSettings{}) {
      std::puts("FAIL: Alternative icons are disabled with Windhawk's default integer values");
      return 8;
    }
    for (auto const &[key, member] : iconSwitches) {
      native::ModSettings expected;
      expected.*member = false;
      native::ReadAlternativeIcons(icons, [key](PCWSTR name) {
        return std::wstring_view(name) == key ? 0 : 1;
      });
      if (icons != expected) {
        std::puts("FAIL: An alternative icon switch did not control its own location");
        return 8;
      }
    }
    std::puts("PASS: Integer icon switches preserve defaults and toggle locations independently");
    if (sound::VolumeIcon(.5f, true) != -1 || sound::VolumeIcon(0.f, false) != 0 ||
        sound::VolumeIcon(.1f, false) != 1 || sound::VolumeIcon(.5f, false) != 2 ||
        sound::VolumeIcon(1.f, false) != 3) {
      std::puts("FAIL: Speaker glyph states distinguish zero volume from mute");
      return 9;
    }
    std::puts("PASS: Speaker glyph states distinguish zero volume from mute");
    winrt::init_apartment();
    for (auto kind : {F::PropertyType::Int32, F::PropertyType::UInt32, F::PropertyType::Single,
                      F::PropertyType::Double}) {
      auto store = std::make_shared<Store>(Store{kind});
      auto model = winrt::make<Model>(store);
      if (!native::SetNumber(model, L"Volume", 0) || store->number != 0 || store->written != kind)
        return 1;
      if (!native::SetNumber(model, L"Volume", 37) || store->number != 37 || store->written != kind)
        return 2;
    }
    std::puts("PASS: native model writes preserve Int32/UInt32/Single/Double types; zero and "
              "restore; no slider binding overwritten");
    return 0;
  } catch (winrt::hresult_error const &error) {
    std::printf("FAIL: %08X\n", unsigned(error.code().value));
    return 3;
  }
}

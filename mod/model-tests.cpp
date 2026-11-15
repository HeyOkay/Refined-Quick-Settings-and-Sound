#define WH_EDITING
#define WH_MOD
#define WH_MOD_ID L"native-model-test"
#include <windhawk_api.h>
#include "quick.h"
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
    if (names != std::vector<std::wstring>{L"foo, inc.", L"steam.exe"} ||
        sound::ParseHidden(L"FOO; Steam.exe, Slack") !=
            std::vector<std::wstring>{L"foo", L"steam.exe", L"slack"}) {
      std::puts("FAIL: Hidden app rows keep punctuation and legacy groups still split");
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

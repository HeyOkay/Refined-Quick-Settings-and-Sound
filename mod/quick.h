#pragma once
#include "common.h"
#include "volume-icon.h"
#include "slider-look.h"
#include "exit.h"
#include "quick-volume.h"
#include "ddc.h"
#include <cstring>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
namespace native {
// This ControlCenter build keeps page height, count and offsets in the native
// PaginatedGridView. Resolve its methods only for the image whose ABI was checked.
using SetQuickRows = void(WINAPI *)(void *, int);
using UpdateQuickExtent = void(WINAPI *)(void *);
inline SetQuickRows setQuickRows = nullptr, originalSetQuickRows = nullptr;
inline UpdateQuickExtent updateQuickExtent = nullptr;
inline void *quickGridVtable = nullptr;
inline void WINAPI KeepQuickRows(void *self, int rows) {
  originalSetQuickRows(self, stopping ? rows : SettingsSnapshot().quickActionRows);
  updateQuickExtent(self);
}
inline void HookQuickRows(HMODULE dll) {
  auto image = reinterpret_cast<BYTE *>(dll);
  auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(image);
  auto nt = reinterpret_cast<IMAGE_NT_HEADERS *>(image + dos->e_lfanew);
  constexpr BYTE signature[] = {0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0xd9,
                                0x39, 0x91, 0xa8, 0, 0, 0, 0x74, 0x5a, 0x89, 0x91};
  if (nt->FileHeader.TimeDateStamp != 1878679008 || nt->OptionalHeader.SizeOfImage != 4313088 ||
      std::memcmp(image + 0x29424c, signature, sizeof(signature)) != 0) {
    Wh_Log(L"Native Quick Settings row override unavailable for this ControlCenter image");
    return;
  }
  auto setter = reinterpret_cast<SetQuickRows>(image + 0x29424c);
  if (*reinterpret_cast<void **>(image + 0x37f518) != image + 0x1018c0)
    return;
  updateQuickExtent = reinterpret_cast<UpdateQuickExtent>(image + 0x2944d0);
  quickGridVtable = image + 0x37f518;
  if (WindhawkUtils::SetFunctionHook(setter, KeepQuickRows, &originalSetQuickRows))
    setQuickRows = setter;
}
// Quick Settings main page (ControlCenter.ControlCenterView). L1Grid holds a
// light Border spanning TogglesGroup + SlidersGroup, then FooterGrid
// (LeftFooter: battery on laptops; RightFooter: the native All settings
// button). Changes:
// - the light Border covers the tiles only; sliders sit in the dark block;
// - Settings text on the left, Display settings and Night light on the right;
// - dark block rows one 38 DIP step apart (StepPx): Settings,
//   [brightness], volume; 32 DIP buttons, 20 DIP side insets, thin sliders;
// - the native battery footer moves into the Settings row;
// - the volume L2 button shows the mixer glyph without the chevron and still
//   runs the native LaunchAdvancedPageCommand;
// - the pager keeps its native place, without arrows, with plain dots.
// Native controls are restored on unload.
class QuickPage {
  X::FrameworkElement view_{nullptr};
  Journal journal_;
  std::vector<std::function<void()>> revoke_;
  std::set<void *> thinSliders_;
  std::set<std::pair<void *, int>> done_;
  ULONGLONG lastEnsure_ = 0;
  bool dirty_ = true, restoring_ = false;
  int pagerTheme_ = -1;
  X::Style normalPip_{nullptr}, selectedPip_{nullptr};
  winrt::weak_ref<X::FrameworkElement> tiledView_;
  C::Grid settingsRow_{nullptr};
  C::StackPanel shortcuts_{nullptr};
  std::vector<sound::QuickShortcut> visibleShortcuts_;
  std::map<sound::QuickShortcut, C::Button> shortcutButtons_;
  C::Button title_{nullptr};
  winrt::weak_ref<C::Button> nativeSettings_;
  HWND host_ = nullptr;
  std::unique_ptr<Ddc> ddc_;
  C::StackPanel header_{nullptr};
  C::Grid brightRow_{nullptr};
  Journal unavailableBrightness_;
  winrt::weak_ref<X::FrameworkElement> hiddenBrightnessRow_;
  winrt::weak_ref<X::FrameworkElement> missingBrightnessSlider_;
  ULONGLONG missingBrightnessSince_ = 0;
  bool nativeBrightnessAvailable_ = false;
  C::Button night_{nullptr}, display_{nullptr};
  X::FrameworkElement brightnessGlyph_{nullptr};
  C::Slider bright_{nullptr};
  U::AnimatedIcon nightIcon_{nullptr};
  U::IAnimatedIconStatics statics_{nullptr};
  bool reapplying_ = false;
  std::wstring nightState_, nightTip_;
  int nightRegistry_ = -1;
  U::PipsPager nightPager_{nullptr};
  int nightStartPage_ = 0;
  int nightSearchPage_ = 0;
  ULONGLONG nightSearchAfter_ = 0;
  U::AnimatedVisualPlayer brightPlayer_{nullptr};
  U::IAnimatedVisualSource2 brightSource_{nullptr};
  double brightFrom_ = 0, brightTo_ = 1;
  HMONITOR monitor_ = nullptr;
  unsigned seenRevision_ = 0;
  ULONGLONG lastRead_ = 0, lastUser_ = 0;
  bool sync_ = false, visible_ = false;
  static inline std::map<HMONITOR, int> brightCache; // -1: no DDC/CI
  template <class R> void Keep(R &&r) {
    auto revoker = std::make_shared<std::decay_t<R>>(std::forward<R>(r));
    revoke_.push_back([revoker] { revoker->revoke(); });
  }
  void Set(X::DependencyObject const &object, X::DependencyProperty const &property,
           IInspectable const &value) {
    journal_.Save(object, property);
    object.SetValue(property, value);
  }
  bool First(IInspectable const &object, int tag) {
    return done_.insert({winrt::get_abi(object), tag}).second;
  }
  SliderLook Look() { return {journal_, revoke_, thinSliders_}; }
  // The volume row's own slider and mute button (QuickVolume).
  std::unique_ptr<QuickVolume> quickVolume_;
  ModSettings settings_;
  C::Slider quickSlider_{nullptr};
  P::ButtonBase quickMute_{nullptr};
  C::Button quickMixerButton_{nullptr};
  IInspectable originalMixerIcon_{nullptr};
  std::shared_ptr<AnimatedVolume> quickIcon_;
  sound::WheelVolume volumeWheel_;
  VolumeToolTip volumeTip_;
  int quickGlyph_ = -2;
  bool quickSync_ = false, quickAvailable_ = false;
  ULONGLONG quickUserAt_ = 0;
  // As ApplyMaster on the Sound page: while the slider is being moved (400 ms
  // after the last own step) the echo of our own writes does not move it back.
  void ApplyVolume(float value, bool muted, bool available) {
    if (!quickSlider_)
      return;
    quickAvailable_ = available;
    bool dragging = GetTickCount64() - quickUserAt_ < 400;
    if (available && !dragging) {
      quickSync_ = true;
      try {
        quickSlider_.Value(muted ? 0 : std::round(value * 100));
      } catch (...) {
      }
      quickSync_ = false;
    }
    if (quickSlider_.IsEnabled() != available)
      quickSlider_.IsEnabled(available);
    if (dragging) {
      value = float(quickSlider_.Value() / 100);
      muted = value <= 0;
    }
    QuickGlyph(value, muted);
  }
  void QuickGlyph(float value, bool muted) {
    if (quickIcon_)
      quickIcon_->Update(value, muted, quickAvailable_);
    else if (int glyph = sound::VolumeIcon(value, muted); quickMute_ && glyph != quickGlyph_) {
      quickMute_.Content(VolumeGlyph(view_, glyph));
      quickGlyph_ = glyph;
    }
  }
  void RefreshVolume() {
    if (!quickSlider_)
      return;
    float value = 0;
    bool muted = false;
    bool ok = quickVolume_ && quickVolume_->Read(value, muted);
    ApplyVolume(value, muted, ok);
  }
  // The media widget above Quick Settings: the shell hides it a few frames
  // after the Sound command runs, and the panel window's visible area follows
  // the content, so anything above the new top edge is cut. The widget fades in
  // 90 ms while RootContent (media + ControlCenterRegion) keeps its height,
  // then the height is released. A longer fade would need the command to wait.
  static void HoldMedia(X::FrameworkElement const &media, int ms) {
    auto content = Parent(media);
    if (!content || content.ActualHeight() < 1)
      return;
    auto local = content.ReadLocalValue(X::FrameworkElement::MinHeightProperty());
    content.MinHeight(content.ActualHeight());
    auto weakContent = winrt::make_weak(content);
    AfterDelay(ms, [weakContent, local] {
      if (auto c = weakContent.get())
        try {
          if (local == X::DependencyProperty::UnsetValue())
            c.ClearValue(X::FrameworkElement::MinHeightProperty());
          else
            c.SetValue(X::FrameworkElement::MinHeightProperty(), local);
        } catch (...) {
        }
    });
  }
  static X::FrameworkElement FindMedia(X::FrameworkElement const &from) {
    if (!from)
      return nullptr;
    X::FrameworkElement top = from;
    for (auto e = Parent(from); e; e = Parent(e))
      top = e;
    return Named(top, L"MediaTransportControls");
  }
  // Brightness -> volume rail and volume rail -> panel edge are one StepPx each
  // at every scale. The native slider items have their own rounding, so the
  // steps are measured after layout and the brightness row height and the
  // grid's bottom margin corrected, at most 4 times per scale.
  C::GridView grid_{nullptr};
  C::Slider volume_{nullptr};
  double rowHeight_ = 39.2, bottomMargin_ = 11.4, alignedScale_ = 0;
  int alignPasses_ = 0;
  void AlignSteps() {
    // Measured only while Quick Settings itself is shown and the view is exactly
    // its height (on the Sound page the view is as tall as Sound). Corrections
    // are limited to +-6 DIP. Cheap exit first: this runs on every layout pass.
    if (alignPasses_ >= 4 && view_ && view_.XamlRoot() &&
        view_.XamlRoot().RasterizationScale() == alignedScale_)
      return;
    if (!grid_ || !volume_ || !view_ || !volume_.IsLoaded() || volume_.ActualHeight() < 1 ||
        !PanelVisible())
      return;
    auto l1 = Named(view_, L"L1Grid");
    if (!l1 || l1.Visibility() != X::Visibility::Visible ||
        std::abs(l1.ActualHeight() - view_.ActualHeight()) > 1)
      return;
    if (auto frame = Named(view_, L"L2Frame");
        frame && frame.Visibility() == X::Visibility::Visible)
      return;
    auto root = view_.XamlRoot();
    if (!root)
      return;
    double s = root.RasterizationScale();
    if (s != alignedScale_) {
      alignedScale_ = s;
      alignPasses_ = 0;
    }
    if (alignPasses_ >= 4)
      return;
    // The pixel row of the drawn 1 DIP rail (SliderLook), not the slider's
    // centre: at 125% the centre falls on half pixels.
    auto centre = [&](X::FrameworkElement const &e) {
      X::FrameworkElement rail = Named(e, L"NativeSoundRail");
      X::FrameworkElement target = rail ? rail : e;
      double y = target.TransformToVisual(view_).TransformPoint({0, 0}).Y;
      if (!rail)
        y += e.ActualHeight() / 2;
      return std::floor(y * s + .5);
    };
    double step = StepPx(s), edge = std::round(view_.ActualHeight() * s), vol = centre(volume_);
    bool changed = false;
    if (std::abs(edge - vol - step) >= 1) {
      bottomMargin_ = std::clamp(bottomMargin_ - (edge - vol - step) / s, 11.4 - 6, 11.4 + 6);
      auto m = grid_.Margin();
      m.Bottom = bottomMargin_;
      grid_.Margin(m);
      changed = true;
    }
    if (bright_ && brightRow_ && brightRow_.Visibility() == X::Visibility::Visible &&
        bright_.ActualHeight() > 0) {
      double b = centre(bright_);
      if (std::abs(vol - b - step) >= 1) {
        rowHeight_ = std::clamp(rowHeight_ + (step - (vol - b)) / s, 39.2 - 6, 39.2 + 6);
        brightRow_.Height(rowHeight_);
        changed = true;
      }
    }
    alignPasses_ = changed ? alignPasses_ + 1 : 4;
  }
  void OpenSettings() {
    try {
      if (auto b = nativeSettings_.get())
        if (auto command = b.Command()) {
          auto parameter = b.CommandParameter();
          if (!parameter)
            parameter = b.DataContext();
          if (command.CanExecute(parameter)) {
            command.Execute(parameter);
            return;
          }
        }
    } catch (...) {
      Wh_Log(L"QuickSettingsCommandError %08X", winrt::to_hresult());
    }
    ShellExecuteW(nullptr, L"open", L"ms-settings:", nullptr, nullptr, SW_SHOWNORMAL);
  }
  void SmallButton(C::Control const &button) {
    Set(button, X::FrameworkElement::WidthProperty(), winrt::box_value(32.));
    Set(button, X::FrameworkElement::HeightProperty(), winrt::box_value(32.));
    Set(button, X::FrameworkElement::MinWidthProperty(), winrt::box_value(0.));
    Set(button, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
    Set(button, X::FrameworkElement::MarginProperty(), winrt::box_value(X::Thickness{4, 0, 4, 0}));
    Set(button, X::FrameworkElement::VerticalAlignmentProperty(),
        winrt::box_value(X::VerticalAlignment::Center));
  }
  C::Grid SettingsRow() {
    // 38 DIP steps: 5.6 + 38.4 lands on whole pixels at 125% (7 + 48 px).
    auto row = Grid(L"Auto,*,Auto");
    row.UseLayoutRounding(true);
    row.Height(38.4);
    row.Margin({0, 5.6, 0, 0});
    auto name = sound::Text(L"Settings", L"Настройки", L"Configuración", L"Einstellungen", L"Configurações", L"设置");
    // The hover frame hugs the text.
    title_ = C::Button();
    if (auto style = Resource(view_, L"IconButtonStyle").try_as<X::Style>())
      title_.Style(style);
    title_.Width(NAN);
    title_.Height(32);
    title_.MinWidth(0);
    title_.MinHeight(32);
    title_.Padding({8, 0, 8, 0});
    title_.Margin({-4, 0, 8, 0});
    title_.HorizontalAlignment(X::HorizontalAlignment::Left);
    title_.HorizontalContentAlignment(X::HorizontalAlignment::Left);
    title_.VerticalContentAlignment(X::VerticalAlignment::Center);
    // IconButtonStyle supplies a symbol font: take the text font from a native tile label.
    C::TextBlock text;
    text.Text(name);
    text.FontSize(14);
    text.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    text.TextTrimming(X::TextTrimming::CharacterEllipsis);
    if (auto label = Named(view_, L"TitleText").try_as<C::TextBlock>())
      text.FontFamily(label.FontFamily());
    else if (auto font = Resource(view_, L"ContentControlThemeFontFamily").try_as<M::FontFamily>())
      text.FontFamily(font);
    title_.Content(text);
    X::Automation::AutomationProperties::SetName(title_, name);
    C::Grid::SetColumn(title_, 0);
    row.Children().Append(title_);
    Keep(title_.Click(winrt::auto_revoke, [this](auto &&, auto &&) { OpenSettings(); }));
    shortcuts_ = C::StackPanel();
    shortcuts_.Orientation(C::Orientation::Horizontal);
    C::Grid::SetColumn(shortcuts_, 2);
    row.Children().Append(shortcuts_);
    display_ = ShortcutButton(sound::QuickShortcut::Display);
    NightButton();
    shortcutButtons_ = {{sound::QuickShortcut::Display, display_}};
    visibleShortcuts_.clear();
    settingsRow_ = row;
    return row;
  }
  U::AnimatedIcon NativeIcon(PCWSTR source) {
    auto factory = NativeAnimatedFactory();
    IInspectable inner{nullptr};
    auto icon = factory.as<U::IAnimatedIconFactory>().CreateInstance(nullptr, inner);
    if (!statics_)
      statics_ = factory.as<U::IAnimatedIconStatics>();
    icon.Source(NativeSource(source));
    icon.Width(16);
    icon.Height(16);
    icon.MirroredWhenRightToLeft(false);
    icon.IsHitTestVisible(false);
    icon.HorizontalAlignment(X::HorizontalAlignment::Center);
    icon.VerticalAlignment(X::VerticalAlignment::Center);
    return icon;
  }
  // Same 32 DIP hover plate as the mute and Sound buttons.
  C::Button IconButton(std::wstring const &name) {
    C::Button b;
    if (auto style = Resource(view_, L"IconButtonStyle").try_as<X::Style>())
      b.Style(style);
    b.Width(32);
    b.Height(32);
    b.MinWidth(0);
    b.MinHeight(0);
    b.Padding({0, 0, 0, 0});
    b.HorizontalAlignment(X::HorizontalAlignment::Center);
    b.VerticalAlignment(X::VerticalAlignment::Center);
    X::Automation::AutomationProperties::SetName(b, name);
    C::ToolTipService::SetToolTip(b, winrt::box_value(name));
    return b;
  }
  C::Button ShortcutButton(sound::QuickShortcut action) {
    using S = sound::QuickShortcut;
    PCWSTR name = nullptr, uri = nullptr;
    wchar_t glyph = 0;
    switch (action) {
    case S::Display:
      name = sound::Text(L"Display settings", L"Параметры дисплея", L"Configuración de pantalla",
                         L"Anzeigeeinstellungen", L"Configurações de exibição", L"显示设置");
      uri = L"ms-settings:display";
      glyph = 0xe7f4;
      break;
    case S::Network:
      name = sound::Text(L"Network & internet", L"Сеть и Интернет", L"Red e Internet",
                         L"Netzwerk und Internet", L"Rede e Internet", L"网络和 Internet");
      uri = L"ms-settings:network-status";
      glyph = 0xe968;
      break;
    case S::Bluetooth:
      name = sound::Text(L"Bluetooth & devices", L"Bluetooth и устройства", L"Bluetooth y dispositivos",
                         L"Bluetooth und Geräte", L"Bluetooth e dispositivos", L"蓝牙和设备");
      uri = L"ms-settings:bluetooth";
      glyph = 0xe702;
      break;
    case S::Apps:
      name = sound::Text(L"Installed apps", L"Установленные приложения", L"Aplicaciones instaladas",
                         L"Installierte Apps", L"Aplicativos instalados", L"已安装的应用");
      uri = L"ms-settings:appsfeatures";
      glyph = 0xe71d;
      break;
    case S::Notifications:
      name = sound::Text(L"Notifications", L"Уведомления", L"Notificaciones",
                         L"Benachrichtigungen", L"Notificações", L"通知");
      uri = L"ms-settings:notifications";
      glyph = 0xea8f;
      break;
    case S::Graphics:
      name = sound::Text(L"Graphics", L"Графика", L"Gráficos", L"Grafik",
                         L"Elementos gráficos", L"图形");
      uri = L"ms-settings:display-advancedgraphics";
      glyph = 0xe7f4;
      break;
    case S::NetworkSettings:
      name = sound::Text(L"Advanced network settings", L"Сетевые параметры",
                         L"Configuración de red avanzada", L"Erweiterte Netzwerkeinstellungen",
                         L"Configurações avançadas de rede", L"高级网络设置");
      uri = L"ms-settings:network-advancedsettings";
      glyph = 0xe839;
      break;
    case S::Power:
      name = sound::Text(L"Power", L"Питание", L"Energía", L"Strom",
                         L"Energia", L"电源");
      uri = L"ms-settings:powersleep";
      glyph = 0xe7e8;
      break;
    case S::Update:
      name = sound::Text(L"Windows Update", L"Центр обновления", L"Windows Update",
                         L"Windows Update", L"Windows Update", L"Windows 更新");
      uri = L"ms-settings:windowsupdate";
      glyph = 0xe777;
      break;
    default:
      return nullptr;
    }
    auto button = IconButton(name);
    button.Margin({4, 0, 4, 0});
    button.Content(SymbolGlyph(view_, glyph));
    Keep(button.Click(winrt::auto_revoke, [uri](auto &&, auto &&) {
      ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL);
    }));
    return button;
  }
  void NightButton() {
    night_ = IconButton(sound::Text(L"Night light", L"Ночной свет", L"Luz nocturna", L"Nachtmodus", L"Luz noturna", L"夜间模式"));
    night_.Margin({4, 0, 4, 0});
    try {
      nightIcon_ = NativeIcon(L"ControlCenter.QA_Nightlight");
      night_.Content(nightIcon_);
    } catch (...) {
      Wh_Log(L"QuickNightIconError %08X", winrt::to_hresult());
      night_.Content(SymbolGlyph(view_, 0xe708));
    }
    // Status only: QA_Nightlight's PointerOver/Pressed frames show the other
    // status, so the icon stays on NormalOn/NormalOff; the button's own hover
    // plate shows the pointer. The button template still writes
    // PointerOver/Normal into the icon through its parents (as with QA_Volume,
    // see AnimatedVolume::Reapply), so a foreign state is replaced at once.
    if (nightIcon_ && statics_) {
      auto icon = nightIcon_;
      auto statics = statics_;
      auto token = nightIcon_.RegisterPropertyChangedCallback(
          statics.StateProperty(),
          [this](X::DependencyObject const &, X::DependencyProperty const &) {
            if (reapplying_ || nightState_.empty())
              return;
            try {
              auto now = std::wstring(statics_.GetState(nightIcon_));
              if (now == nightState_)
                return;
              reapplying_ = true;
              statics_.SetState(nightIcon_, nightState_);
              reapplying_ = false;
            } catch (...) {
              reapplying_ = false;
            }
          });
      revoke_.push_back([icon, statics, token] {
        icon.UnregisterPropertyChangedCallback(statics.StateProperty(), token);
      });
      // Every opening reloads the icon and its player can start on the default
      // frame while State keeps its value (no change, no callback): jump to the
      // current status ("" first, so no Off->On transition plays).
      Keep(nightIcon_.Loaded(winrt::auto_revoke, [this](auto &&, auto &&) {
        if (nightState_.empty() || !statics_ || !nightIcon_)
          return;
        reapplying_ = true;
        try {
          statics_.SetState(nightIcon_, L"");
          statics_.SetState(nightIcon_, nightState_);
        } catch (...) {
        }
        reapplying_ = false;
      }));
    }
    Keep(night_.Click(winrt::auto_revoke, [this](auto &&, auto &&) { ToggleNight(); }));
    nightRegistry_ = RegistryNight();
    NightState();
  }
  C::Grid BrightnessRow() {
    auto row = Grid(L"40,*,40");
    row.Height(39.2);
    row.Visibility(X::Visibility::Collapsed);
    try {
      BrightnessIcon();
      brightnessGlyph_ = brightPlayer_;
    } catch (...) {
      Wh_Log(L"QuickBrightnessPlayerError %08X", winrt::to_hresult());
      brightnessGlyph_ = SymbolGlyph(view_, 0xe706);
    }
    row.Children().Append(brightnessGlyph_);
    bright_ = C::Slider();
    if (auto style = Resource(view_, L"HeaderIconSliderStyle").try_as<X::Style>())
      bright_.Style(style);
    bright_.MinWidth(0);
    bright_.Minimum(0);
    bright_.Maximum(100);
    bright_.StepFrequency(1);
    bright_.SmallChange(2);
    bright_.LargeChange(10);
    bright_.IsThumbToolTipEnabled(true);
    X::Automation::AutomationProperties::SetName(bright_, sound::Text(L"Brightness", L"Яркость", L"Brillo", L"Helligkeit", L"Brilho", L"亮度"));
    C::Grid::SetColumn(bright_, 1);
    row.Children().Append(bright_);
    Keep(bright_.ValueChanged(winrt::auto_revoke, [this](auto &&, auto const &args) {
      if (sync_ || !ddc_)
        return;
      int value = int(std::lround(args.NewValue()));
      lastUser_ = GetTickCount64();
      brightCache[monitor_] = value;
      ddc_->Write(monitor_, value);
      ShowBrightness(value);
    }));
    Keep(bright_.PointerWheelChanged(
        winrt::auto_revoke, [this](auto &&, X::Input::PointerRoutedEventArgs const &args) {
          try {
            if (args.Handled())
              return;
            auto point = args.GetCurrentPoint(bright_);
            if (point.Properties().IsHorizontalMouseWheel())
              return;
            double delta = point.Properties().MouseWheelDelta() / 120.;
            if (!delta)
              return;
            bright_.Value(std::clamp(bright_.Value() + delta * bright_.SmallChange(),
                                     bright_.Minimum(), bright_.Maximum()));
            args.Handled(true);
          } catch (...) {
          }
        }));
    brightRow_ = row;
    return row;
  }
  // QA_Brightness has two markers, Brightness_at_0 and Brightness_at_100: the
  // icon is drawn for a level between them (the native slider's player). An
  // AnimatedVisualPlayer shows it at the slider's level; AnimatedIcon could only
  // snap to the two ends.
  void BrightnessIcon() {
    auto source = NativeSource(L"ControlCenter.QA_Brightness");
    brightSource_ = source;
    if (auto markers = source.Markers()) {
      if (markers.HasKey(L"Brightness_at_0"))
        brightFrom_ = markers.Lookup(L"Brightness_at_0");
      if (markers.HasKey(L"Brightness_at_100"))
        brightTo_ = markers.Lookup(L"Brightness_at_100");
    }
    winrt::Windows::Foundation::IActivationFactory factory{nullptr};
    try {
      factory = winrt::get_activation_factory<U::AnimatedVisualPlayer>();
    } catch (...) {
      auto mux = GetModuleHandleW(L"Microsoft.UI.Xaml.dll");
      if (!mux)
        throw;
      factory = NativeFactory(L"Microsoft.UI.Xaml.Controls.AnimatedVisualPlayer", mux);
    }
    // Composable class: IActivationFactory::ActivateInstance returns E_NOTIMPL.
    IInspectable inner{nullptr};
    brightPlayer_ = factory.as<U::IAnimatedVisualPlayerFactory>().CreateInstance(nullptr, inner);
    brightPlayer_.AutoPlay(false);
    brightPlayer_.Source(source.as<U::IAnimatedVisualSource>());
    brightPlayer_.Width(16);
    brightPlayer_.Height(16);
    brightPlayer_.IsHitTestVisible(false);
    brightPlayer_.HorizontalAlignment(X::HorizontalAlignment::Center);
    brightPlayer_.VerticalAlignment(X::VerticalAlignment::Center);
  }
  void ShowBrightness(int value) {
    if (!brightPlayer_)
      return;
    try {
      if (brightSource_) {
        bool light = view_.ActualTheme() == X::ElementTheme::Light;
        brightSource_.SetColorProperty(L"Foreground",
                                       light ? winrt::Windows::UI::Color{0xE4, 0, 0, 0}
                                             : winrt::Windows::UI::Color{0xFF, 0xFF, 0xFF, 0xFF});
      }
      brightPlayer_.SetProgress(brightFrom_ +
                                (brightTo_ - brightFrom_) * std::clamp(value, 0, 100) / 100.);
    } catch (...) {
    }
  }
  // Night light status, read (never written) from the shell's CloudStore state
  // (byte 18: 0x15 on, 0x13 off).
  static int RegistryNight() {
    constexpr auto key =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store\\DefaultAccount\\Current"
        L"\\default$windows.data.bluelightreduction.bluelightreductionstate\\windows.data."
        L"bluelightreduction.bluelightreductionstate";
    BYTE data[256];
    DWORD size = sizeof(data);
    if (RegGetValueW(HKEY_CURRENT_USER, key, L"Data", RRF_RT_REG_BINARY, nullptr, data, &size) !=
            ERROR_SUCCESS ||
        size < 19)
      return -1;
    return data[18] == 0x15 ? 1 : data[18] == 0x13 ? 0 : -1;
  }
  bool NightOn() { return nightRegistry_ == 1; }
  void NightState() {
    if (!night_)
      return;
    bool on = NightOn();
    auto state = std::wstring(on ? L"NormalOn" : L"NormalOff");
    if (nightIcon_ && statics_ && state != nightState_) {
      nightState_ = state;
      reapplying_ = true;
      try {
        statics_.SetState(nightIcon_, state);
      } catch (...) {
      }
      reapplying_ = false;
    }
    std::wstring name = nightRegistry_ < 0 ? sound::Text(L"Night light", L"Ночной свет", L"Luz nocturna", L"Nachtmodus", L"Luz noturna", L"夜间模式")
                    : on ? sound::Text(L"Night light: on", L"Ночной свет включен", L"Luz nocturna activada", L"Nachtmodus ein", L"Luz noturna ativada", L"夜间模式已打开")
                         : sound::Text(L"Night light: off", L"Ночной свет выключен", L"Luz nocturna desactivada", L"Nachtmodus aus", L"Luz noturna desativada", L"夜间模式已关闭");
    if (name != nightTip_) {
      nightTip_ = name;
      C::ToolTipService::SetToolTip(night_, winrt::box_value(name));
      X::Automation::AutomationProperties::SetName(night_, name);
    }
  }
  void PollNight() {
    int night = RegistryNight();
    if (night != nightRegistry_) {
      nightRegistry_ = night;
      NightState();
    }
  }
  static void *Identity(IInspectable const &o) {
    return o ? winrt::get_abi(o.as<winrt::Windows::Foundation::IUnknown>()) : nullptr;
  }
  // Tile containers are recycled as the pager changes pages.
  X::FrameworkElement NightTileNow() {
    void *mine = nightIcon_ ? Identity(nightIcon_) : nullptr;
    X::FrameworkElement found{nullptr};
    std::function<void(X::DependencyObject const &)> walk = [&](X::DependencyObject const &o) {
      if (found)
        return;
      if (auto a = o.try_as<U::AnimatedIcon>())
        if (Identity(a) != mine && Class(a.Source()) == L"ControlCenter.QA_Nightlight") {
          found = a.as<X::FrameworkElement>();
          return;
        }
      for (int i = 0; i < M::VisualTreeHelper::GetChildrenCount(o); i++)
        walk(M::VisualTreeHelper::GetChild(o, i));
    };
    walk(view_);
    if (!found)
      return nullptr;
    for (auto p = Parent(found); p && p != view_; p = Parent(p))
      if (auto toggle = p.try_as<P::ToggleButton>())
        return toggle.IsEnabled() ? p : nullptr;
    return nullptr;
  }
  bool ToggleNightTile() {
    try {
      if (auto tile = NightTileNow())
        if (auto peer =
                X::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(tile)) {
          if (auto toggle = peer.GetPattern(X::Automation::Peers::PatternInterface::Toggle)
                                .try_as<X::Automation::Provider::IToggleProvider>()) {
            toggle.Toggle();
            return true;
          }
          if (auto invoke = peer.GetPattern(X::Automation::Peers::PatternInterface::Invoke)
                                .try_as<X::Automation::Provider::IInvokeProvider>()) {
            invoke.Invoke();
            return true;
          }
        }
    } catch (...) {
      Wh_Log(L"QuickNightToggleError %08X", winrt::to_hresult());
    }
    return false;
  }
  static void OpenNightSettings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:nightlight", nullptr, nullptr, SW_SHOWNORMAL);
  }
  void FinishNightSearch() {
    auto pager = std::exchange(nightPager_, nullptr);
    if (pager && pager.SelectedPageIndex() == nightSearchPage_)
      pager.SelectedPageIndex(nightStartPage_);
  }
  void NextNightPage() {
    nightSearchPage_ = (nightSearchPage_ + 1) % nightPager_.NumberOfPages();
    nightPager_.SelectedPageIndex(nightSearchPage_);
    // Let the shell realize the next page before looking for its tile.
    nightSearchAfter_ = GetTickCount64() + 200;
  }
  void ToggleNight() {
    if (nightPager_ || ToggleNightTile())
      return;
    try {
      auto pager = Named(view_, L"QuickActionsPager").try_as<U::PipsPager>();
      if (pager && pager.NumberOfPages() > 1 && pager.SelectedPageIndex() >= 0) {
        nightStartPage_ = nightSearchPage_ = pager.SelectedPageIndex();
        nightPager_ = pager;
        NextNightPage();
        return;
      }
    } catch (...) {
      nightPager_ = nullptr;
      Wh_Log(L"QuickNightPagerError %08X", winrt::to_hresult());
    }
    OpenNightSettings();
  }
  void ContinueNightSearch() {
    if (!nightPager_)
      return;
    if (!visible_ || nightPager_.SelectedPageIndex() != nightSearchPage_) {
      FinishNightSearch();
      return;
    }
    if (GetTickCount64() < nightSearchAfter_)
      return;
    if (ToggleNightTile()) {
      FinishNightSearch();
      return;
    }
    int pages = nightPager_.NumberOfPages();
    if (pages > 1 && (nightSearchPage_ + 1) % pages != nightStartPage_) {
      NextNightPage();
      return;
    }
    FinishNightSearch();
    OpenNightSettings();
  }
  X::FrameworkElement NativeBrightnessRow() {
    auto group = Named(view_, L"SlidersGroup");
    auto icon = Named(group, L"BrightnessPlayer");
    if (!icon)
      return nullptr;
    for (auto parent = Parent(icon); parent && parent != group; parent = Parent(parent))
      if (parent.try_as<C::GridViewItem>())
        return parent;
    return nullptr;
  }
  static C::Slider NativeBrightnessSlider(X::FrameworkElement const &row) {
    return Find(row, [](auto const &element) { return !!element.template try_as<C::Slider>(); })
        .try_as<C::Slider>();
  }
  bool NativeBrightnessUsable(C::Slider const &slider) {
    if (!slider || !slider.IsEnabled() || slider.Maximum() <= slider.Minimum())
      return false;
    for (auto element = slider.as<X::FrameworkElement>(); element; element = Parent(element)) {
      if (element.Visibility() != X::Visibility::Visible)
        return false;
      if (element == view_)
        return true;
    }
    return false;
  }
  void HideUnavailableBrightness(X::FrameworkElement const &row, bool hide) {
    auto hidden = hiddenBrightnessRow_.get();
    if (hidden && row == hidden && hide)
      return;
    unavailableBrightness_.Restore();
    hiddenBrightnessRow_ = nullptr;
    if (!hide)
      return;
    // Keep the native visibility binding and slider state live so a monitor
    // change can bring the native row back while Quick Settings is open.
    unavailableBrightness_.Save(row, X::FrameworkElement::MinHeightProperty());
    unavailableBrightness_.Save(row, X::FrameworkElement::MaxHeightProperty());
    unavailableBrightness_.Save(row, X::UIElement::IsHitTestVisibleProperty());
    hiddenBrightnessRow_ = winrt::make_weak(row);
    row.MinHeight(0);
    row.MaxHeight(0);
    row.IsHitTestVisible(false);
  }
  bool BrightnessTemplateMissing(X::FrameworkElement const &row, C::Slider const &slider) {
    if (!row || slider || row.Visibility() != X::Visibility::Visible) {
      missingBrightnessSlider_ = nullptr;
      return false;
    }
    if (hiddenBrightnessRow_.get() == row)
      return true;
    if (!PanelVisible()) {
      missingBrightnessSlider_ = nullptr;
      return false;
    }
    // The shell can load the icon before the slider. Leave this row alone for
    // one layout interval, then hide it unless the real control has arrived.
    auto now = GetTickCount64();
    if (missingBrightnessSlider_.get() != row) {
      missingBrightnessSlider_ = winrt::make_weak(row);
      missingBrightnessSince_ = now;
    }
    return now - missingBrightnessSince_ >= 250;
  }
  void PlaceDisplayActions() {
    if (!settingsRow_ || !night_ || !display_)
      return;
    auto nativeRow = NativeBrightnessRow();
    if (!settings_.brightnessEnabled && nativeRow && First(nativeRow, 76))
      Set(nativeRow, X::UIElement::VisibilityProperty(), winrt::box_value(X::Visibility::Collapsed));
    auto nativeSlider = NativeBrightnessSlider(nativeRow);
    bool nativeVisible = settings_.brightnessEnabled && NativeBrightnessUsable(nativeSlider);
    bool missingSlider = settings_.brightnessEnabled &&
                         BrightnessTemplateMissing(nativeRow, nativeSlider);
    HideUnavailableBrightness(nativeRow, settings_.brightnessEnabled && nativeRow &&
                                            nativeRow.Visibility() == X::Visibility::Visible &&
                                            !nativeVisible && (nativeSlider || missingSlider));
    if (nativeVisible && brightRow_ && brightRow_.Visibility() == X::Visibility::Visible) {
      brightRow_.Visibility(X::Visibility::Collapsed);
      alignPasses_ = 0;
    }
    bool nativeDisplay = nativeVisible && Find(nativeRow, [](auto const &e) {
      auto button = e.template try_as<C::Button>();
      return button && button.Visibility() == X::Visibility::Visible &&
             String(button.CommandParameter()) == L"ms-settings:display";
    });
    bool ddcVisible = brightRow_ && brightRow_.Visibility() == X::Visibility::Visible;
    auto shortcuts = sound::QuickShortcuts(settings_.quickShortcuts, nativeDisplay || ddcVisible);
    bool changed = shortcuts != visibleShortcuts_;
    if (changed)
      shortcuts_.Children().Clear();
    if (brightRow_) {
      auto place = [&](C::Button const &button, int column) {
        uint32_t index = 0;
        bool attached = brightRow_.Children().IndexOf(button, index);
        if (ddcVisible && !attached) {
          C::Grid::SetColumn(button, column);
          brightRow_.Children().Append(button);
        } else if (!ddcVisible && attached) {
          brightRow_.Children().RemoveAt(index);
        }
      };
      place(display_, 2);
      place(night_, 0);
    }
    if (changed) {
      for (auto action : shortcuts) {
        auto it = shortcutButtons_.find(action);
        if (it == shortcutButtons_.end())
          it = shortcutButtons_.emplace(action, ShortcutButton(action)).first;
        if (it->second)
          shortcuts_.Children().Append(it->second);
      }
      visibleShortcuts_ = std::move(shortcuts);
    }
    if (brightnessGlyph_) {
      auto visibility = ddcVisible ? X::Visibility::Collapsed : X::Visibility::Visible;
      if (brightnessGlyph_.Visibility() != visibility)
        brightnessGlyph_.Visibility(visibility);
    }
  }
  bool PanelVisible() { return WindowShown(host_); }
  void ApplyBright(int value, bool nativeAvailable) {
    if (!brightRow_)
      return;
    bool show = value >= 0 && !nativeAvailable;
    auto want = show ? X::Visibility::Visible : X::Visibility::Collapsed;
    if (brightRow_.Visibility() != want) {
      brightRow_.Visibility(want);
      alignPasses_ = 0;
      PlaceDisplayActions();
    }
    if (value >= 0 && std::lround(bright_.Value()) != value) {
      sync_ = true;
      bright_.Value(value);
      sync_ = false;
    }
    if (show)
      ShowBrightness(value);
  }
  // Panel monitor: cached value at once, a DDC/CI read on opening and every 3 s
  // while open (not while the user is changing it).
  void Brightness(bool opening) {
    if (!brightRow_ || !ddc_)
      return;
    auto now = GetTickCount64();
    auto monitor = MonitorFromWindow(host_, MONITOR_DEFAULTTONEAREST);
    bool nativeAvailable = NativeBrightnessUsable(NativeBrightnessSlider(NativeBrightnessRow()));
    bool availabilityChanged = nativeBrightnessAvailable_ != nativeAvailable;
    nativeBrightnessAvailable_ = nativeAvailable;
    if (monitor != monitor_ || availabilityChanged) {
      monitor_ = monitor;
      lastRead_ = 0;
      auto cached = brightCache.find(monitor);
      ApplyBright(cached != brightCache.end() ? cached->second : -1, nativeAvailable);
    }
    if (!nativeAvailable && (opening || now - lastRead_ > 3000) && now - lastUser_ > 1500 &&
        (opening || visible_)) {
      lastRead_ = now;
      ddc_->Read(monitor_);
    }
    auto r = ddc_->Latest();
    if (r.revision != seenRevision_) {
      seenRevision_ = r.revision;
      if (r.monitor == monitor_) {
        int value = r.supported ? r.value : -1;
        brightCache[monitor_] = value;
        if (now - lastUser_ > 1500 || !r.supported)
          ApplyBright(value, nativeAvailable);
      }
    }
  }

  // Move the shell's actual footer ItemsControl, not a copy of its battery icon.
  // Keep its item templates, commands, bindings and future collection changes.
  bool MoveBatteryFooter(X::FrameworkElement const &left, X::FrameworkElement const &footer) {
    if (!left || !settingsRow_) {
      if (settingsRow_ && First(view_, 72))
        Wh_Log(L"Quick battery: LeftFooter was not found");
      return false;
    }
    // A template root can be owned by a ContentPresenter rather than directly
    // by a Panel. Move its enclosing slot; never remove generated ItemsPanel
    // children, whose collection is controlled by the ItemsControl.
    for (auto node = left; node; node = Parent(node))
      if (node == settingsRow_)
        return true;
    auto moving = left;
    C::Panel parent{nullptr};
    uint32_t index = 0;
    for (auto node = left; node && node != footer; node = Parent(node)) {
      auto candidate = Parent(node).try_as<C::Panel>();
      if (candidate && !candidate.IsItemsHost() && candidate.Children().IndexOf(node, index)) {
        moving = node;
        parent = candidate;
        break;
      }
    }
    if (!parent) {
      if (First(left, 73))
        Wh_Log(L"Quick battery: no detachable slot, left=%s parent=%s footer=%s",
               Class(left).c_str(), Class(Parent(left)).c_str(), Class(footer).c_str());
      return false;
    }
    auto element = moving;
    // Preserve the former inheritance source, including subsequent DataContext
    // replacements. Never overwrite an explicit native DataContext/binding.
    if (element.ReadLocalValue(X::FrameworkElement::DataContextProperty()) ==
        X::DependencyProperty::UnsetValue()) {
      journal_.Save(element, X::FrameworkElement::DataContextProperty());
      D::Binding binding;
      binding.Source(parent);
      binding.Path(X::PropertyPath(L"DataContext"));
      binding.Mode(D::BindingMode::OneWay);
      element.SetBinding(X::FrameworkElement::DataContextProperty(), binding);
    }
    Set(element, C::Grid::ColumnProperty(), winrt::box_value(1));
    Set(element, C::Grid::RowProperty(), winrt::box_value(0));
    Set(element, C::Grid::ColumnSpanProperty(), winrt::box_value(1));
    Set(element, C::Grid::RowSpanProperty(), winrt::box_value(1));
    Set(element, X::FrameworkElement::WidthProperty(), winrt::box_value(static_cast<double>(NAN)));
    Set(element, X::FrameworkElement::MinWidthProperty(), winrt::box_value(0.));
    Set(element, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
    Set(element, X::FrameworkElement::MarginProperty(), winrt::box_value(X::Thickness{0, 0, 6, 0}));
    Set(element, X::FrameworkElement::HorizontalAlignmentProperty(),
        winrt::box_value(X::HorizontalAlignment::Right));
    Set(element, X::FrameworkElement::VerticalAlignmentProperty(),
        winrt::box_value(X::VerticalAlignment::Center));
    auto oldParent = winrt::make_weak(parent);
    auto newParent = winrt::make_weak(settingsRow_);
    // Registered before the mutation, so a failed Append is reversible as well.
    journal_.undo.push_back([oldParent, newParent, element, index] {
      if (auto dest = newParent.get()) {
        uint32_t current = 0;
        if (dest.Children().IndexOf(element, current))
          dest.Children().RemoveAt(current);
      }
      if (auto source = oldParent.get(); source && !Parent(element)) {
        uint32_t current = 0;
        if (!source.Children().IndexOf(element, current))
          source.Children().InsertAt(std::min(index, source.Children().Size()), element);
      }
    });
    parent.Children().RemoveAt(index);
    settingsRow_.Children().Append(element);
    Wh_Log(L"Quick battery: moved %s from %s into All settings",
           Class(element).c_str(), Class(parent).c_str());
    return true;
  }
  void CompactBattery(X::FrameworkElement const &left) {
    // Match the gear: 36-DIP button, with a 2-DIP template inset on every
    // side, giving both native hover plates the same 32-DIP height.
    Find(left, [&](auto const &element) {
      if (auto button = element.template try_as<C::Button>()) {
        if (First(button, 70)) {
          Set(button, X::FrameworkElement::HeightProperty(), winrt::box_value(36.));
          Set(button, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
          Set(button, X::FrameworkElement::MinWidthProperty(), winrt::box_value(0.));
          Set(button, X::FrameworkElement::MarginProperty(), winrt::box_value(X::Thickness{}));
          Set(button, C::Control::PaddingProperty(), winrt::box_value(X::Thickness{8, 0, 8, 0}));
          Set(button, C::Control::HorizontalContentAlignmentProperty(),
              winrt::box_value(X::HorizontalAlignment::Center));
          Set(button, C::Control::VerticalContentAlignmentProperty(),
              winrt::box_value(X::VerticalAlignment::Center));
          Set(button, X::FrameworkElement::VerticalAlignmentProperty(),
              winrt::box_value(X::VerticalAlignment::Center));
        }
        // Remove the asymmetric 4,3,2,3 footer ItemsControl margin and any
        // enclosing slot insets. The plate, not the percentage, ends at the
        // slider rail: 6 DIP outside the button + 2 DIP inside its template.
        for (auto slot = Parent(button); slot && slot != settingsRow_; slot = Parent(slot))
          if (First(slot, 75)) {
            bool outer = Parent(slot) == settingsRow_;
            Set(slot, X::FrameworkElement::MarginProperty(),
                winrt::box_value(X::Thickness{0, 0, outer ? 6. : 0., 0}));
            Set(slot, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
            Set(slot, X::FrameworkElement::VerticalAlignmentProperty(),
                winrt::box_value(X::VerticalAlignment::Center));
          }
        if (auto presenter = Named(button, L"ContentPresenter"); presenter && First(presenter, 71))
          Set(presenter, X::FrameworkElement::MarginProperty(), winrt::box_value(X::Thickness{2, 2, 2, 2}));
        // Keep the native icon/font/bindings, but centre both icon branches and
        // the percentage on the gear axis (the new percentage has a -1 top inset).
        if (auto content = button.Content().template try_as<X::FrameworkElement>()) {
          if (First(content, 76))
            Set(content, X::FrameworkElement::VerticalAlignmentProperty(),
                winrt::box_value(X::VerticalAlignment::Center));
          Find(content, [&](auto const &part) {
            if (part.template try_as<C::TextBlock>() && First(part, 77)) {
              auto margin = part.Margin();
              margin.Top = margin.Bottom = 0;
              Set(part, X::FrameworkElement::MarginProperty(), winrt::box_value(margin));
              Set(part, X::FrameworkElement::VerticalAlignmentProperty(),
                  winrt::box_value(X::VerticalAlignment::Center));
            }
            return false;
          });
        }
      }
      return false; // Visit all native buttons without stopping the traversal.
    });
    // Reserve the occupied action slots and the battery width.
    if (title_ && settingsRow_.ActualWidth() > 0) {
      double available = std::max(
          0., settingsRow_.ActualWidth() - shortcuts_.ActualWidth() - left.ActualWidth() - 6. - 12.);
      if (title_.MaxWidth() != available)
        title_.MaxWidth(available);
    }
  }
  void Footer() {
    auto footer = Named(view_, L"FooterGrid");
    if (!footer) {
      if (First(view_, 74)) Wh_Log(L"Quick battery: FooterGrid was not found");
      return;
    }
    if (!nativeSettings_.get()) {
      auto right = Named(footer, L"RightFooter");
      if (auto icon = Find(right ? right : footer, [](auto const &e) {
            auto a = e.template try_as<U::AnimatedIcon>();
            return a && Class(a.Source()) == L"ControlCenter.QA_All_Settings";
          }))
        for (auto p = Parent(icon); p && p != footer; p = Parent(p))
          if (auto button = p.try_as<C::Button>()) {
            nativeSettings_ = winrt::make_weak(button);
            break;
          }
    }
    // Search the view: after migration LeftFooter lives in our header.
    auto left = Named(view_, L"LeftFooter");
    bool moved = MoveBatteryFooter(left, footer);
    if (moved)
      CompactBattery(left);
    bool used = left && ((left.try_as<C::ItemsControl>() &&
                         left.as<C::ItemsControl>().Items().Size() > 0) || Find(left, [&](auto const &e) {
                  return e != left && e.template try_as<C::ItemsControl>() &&
                         e.Visibility() == X::Visibility::Visible;
                }));
    // Preserve the native fallback if an unfamiliar parent prevents migration,
    // or if no working replacement for the native Settings button was found.
    auto want = (!moved && used) || !nativeSettings_.get()
                    ? X::Visibility::Visible : X::Visibility::Collapsed;
    if (footer.Visibility() != want)
      Set(footer, X::UIElement::VisibilityProperty(), winrt::box_value(want));
  }
  // The light Border spans the tiles and the sliders rows: end it above the sliders.
  void DarkBlock(C::Grid const &l1) {
    auto sliders = Named(l1, L"SlidersGroup");
    if (!sliders)
      return;
    int row = C::Grid::GetRow(sliders);
    for (auto const &child : l1.Children())
      if (auto border = child.try_as<C::Border>(); border && border.Name().empty()) {
        int top = C::Grid::GetRow(border), span = C::Grid::GetRowSpan(border);
        if (top < row && top + span > row)
          Set(border, C::Grid::RowSpanProperty(), winrt::box_value(row - top));
        // Its edge divides the tiles from the dark block: the divider colour for
        // the slider rails and the Sound page's SV/VM divider.
        if (top < row) {
          quickDivider = winrt::make_weak(border);
        }
      }
  }
  void Pager() {
    auto pager = Named(view_, L"QuickActionsPager").try_as<U::PipsPager>();
    if (!pager)
      return;
    if (First(pager, 1)) {
      normalPip_ = pager.NormalPipStyle();
      selectedPip_ = pager.SelectedPipStyle();
      Set(pager, U::PipsPager::PreviousButtonVisibilityProperty(),
          winrt::box_value(U::PipsPagerButtonVisibility::Collapsed));
      Set(pager, U::PipsPager::NextButtonVisibilityProperty(),
          winrt::box_value(U::PipsPagerButtonVisibility::Collapsed));
    }
    double pipScale = pager.XamlRoot() ? pager.XamlRoot().RasterizationScale() : 1.;
    int theme = (pager.ActualTheme() == X::ElementTheme::Light ? 1 : 0) |
                (int(std::lround(pipScale * 100)) << 1);
    if (theme == pagerTheme_)
      return;
    pagerTheme_ = theme;
    theme &= 1;
    // Dots in the text colour, inactive at 35%, 10 DIP apart. Position stays
    // native: centred between the tiles and the panel edge.
    winrt::Windows::UI::Color color = theme ? winrt::Windows::UI::Color{0xE4, 0, 0, 0}
                                            : winrt::Windows::UI::Color{0xFF, 0xF6, 0xF6, 0xF6};
    auto pip = [&](X::Style const &base, double opacity, double size) {
      X::Style style(winrt::xaml_typename<C::Button>());
      if (base)
        style.BasedOn(base);
      M::SolidColorBrush brush(color);
      brush.Opacity(opacity);
      style.Setters().Append(X::Setter(C::Control::ForegroundProperty(), brush));
      style.Setters().Append(
          X::Setter(X::FrameworkElement::HeightProperty(), winrt::box_value(10.)));
      style.Setters().Append(
          X::Setter(X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.)));
      style.Setters().Append(X::Setter(C::Control::FontSizeProperty(), winrt::box_value(size)));
      return style;
    };
    // Both dots on one vertical axis: their pixel sizes must have the same
    // parity (at 100%, 4 and 5 px cannot be centred together; 4 and 6 can).
    double normalPx = std::max(2., std::round(4 * pipScale)), selectedPx = std::round(5 * pipScale);
    if (int(selectedPx - normalPx) % 2)
      selectedPx += 1;
    Set(pager, U::PipsPager::NormalPipStyleProperty(), pip(normalPip_, .35, normalPx / pipScale));
    Set(pager, U::PipsPager::SelectedPipStyleProperty(),
        pip(selectedPip_, 1., selectedPx / pipScale));
  }
  void TileRows(C::Grid const &l1) {
    if (!setQuickRows)
      return;
    auto paginated = Find(Named(l1, L"TogglesGroup"), [](auto const &e) {
      return Class(e) == L"ControlCenter.PaginatedGridView";
    });
    if (!paginated || paginated == tiledView_.get())
      return;
    // The IPaginatedGridView subobject at +0x10 is returned by this image's
    // QueryInterface. Verify the complete object's vtable before calling it.
    constexpr GUID iid{0xab1ce38d, 0x0d8e, 0x55a0, {0x87, 0xb5, 0x89, 0x0c, 0xd5, 0x4f, 0x67, 0x22}};
    winrt::com_ptr<::IUnknown> iface;
    if (FAILED(winrt::get_unknown(paginated)->QueryInterface(iid, iface.put_void())))
      return;
    auto self = reinterpret_cast<BYTE *>(iface.get()) - 0x10;
    if (*reinterpret_cast<void **>(self) != quickGridVtable)
      return;
    int rows = *reinterpret_cast<int const *>(self + 0xa8);
    auto grid = Named(paginated, L"RootGridView").try_as<C::GridView>();
    if (!grid)
      return;
    // Native layouts use one or two rows depending on available height.
    int nativeRows = rows == 1 ? 1 : 2;
    if (auto pager = Named(view_, L"QuickActionsPager").try_as<U::PipsPager>();
        pager && rows != settings_.quickActionRows && pager.SelectedPageIndex() > 0)
      pager.SelectedPageIndex(0);
    setQuickRows(self, settings_.quickActionRows);
    tiledView_ = winrt::make_weak(paginated);
    Wh_Log(L"Quick Settings pagination set to %d rows, %u tiles", settings_.quickActionRows,
           grid.Items().Size());
    journal_.undo.push_back([iface, nativeRows] {
      auto self = reinterpret_cast<BYTE *>(iface.get()) - 0x10;
      originalSetQuickRows(self, nativeRows);
      updateQuickExtent(self);
    });
  }
  // Mute button of the volume row: a button of the mod's own in the same Grid
  // cell, with the system QA_Volume asset driven by Core Audio and the slider.
  // The mod's icon cannot live inside the native ToggleButton: the shell owns
  // that button and puts its own icon and Mute state back.
  // The volume row: the mod's own slider (QuickVolume) and mute button. The
  // native slider stays in the layout, transparent, without hit testing, tab
  // stop or UI Automation presence, so the shell's row keeps its size; the
  // native ToggleButton leaves the row and is put back on unload.
  void VolumeIcon(C::Button const &l2) {
    auto row = Parent(l2).try_as<C::Panel>();
    if (!row)
      return;
    P::ToggleButton mute{nullptr};
    C::Slider slider{nullptr};
    for (auto const &child : row.Children()) {
      if (auto t = child.try_as<P::ToggleButton>())
        mute = t;
      if (auto v = child.try_as<C::Slider>(); v && v != quickSlider_)
        slider = v;
    }
    if (!mute || !slider || !First(mute, 5))
      return;
    if (settings_.altQuickVolume)
      try {
        quickIcon_ = std::make_shared<AnimatedVolume>();
      } catch (...) {
        Wh_Log(L"QuickVolumeIconError %08X", winrt::to_hresult());
      }
    auto hide = [this](X::UIElement const &e) {
      Set(e, X::UIElement::OpacityProperty(), winrt::box_value(0.));
      Set(e, X::UIElement::IsHitTestVisibleProperty(), winrt::box_value(false));
      Set(e, C::Control::IsTabStopProperty(), winrt::box_value(false));
      Set(e, X::Automation::AutomationProperties::AccessibilityViewProperty(),
          winrt::box_value(X::Automation::Peers::AccessibilityView::Raw));
    };
    auto insertAfter = [this, row](X::UIElement const &native, X::UIElement const &own) {
      uint32_t index = 0;
      row.Children().IndexOf(native, index);
      row.Children().InsertAt(index + 1, own);
      auto weakRow = winrt::make_weak(row);
      journal_.undo.push_back([weakRow, own] {
        if (auto r = weakRow.get()) {
          uint32_t i = 0;
          if (r.Children().IndexOf(own, i))
            r.Children().RemoveAt(i);
        }
      });
    };
    // Both glyphs share the Windows toggle and its original command.
    journal_.Save(mute, C::ContentControl::ContentProperty());
    quickMute_ = mute;
    quickGlyph_ = -2;
    if (quickIcon_)
      mute.Content(quickIcon_->Isolated());
    // Slider: as master_ on the Sound page; geometry and thin look follow in Sliders().
    C::Slider mine;
    if (auto style = Resource(view_, L"HeaderIconSliderStyle").try_as<X::Style>())
      mine.Style(style);
    mine.Name(L"NativeQuickVolume");
    mine.MinWidth(0);
    mine.MinHeight(0);
    mine.Height(32);
    mine.Minimum(0);
    mine.Maximum(100);
    mine.StepFrequency(1);
    mine.SmallChange(slider.SmallChange() > 0 ? slider.SmallChange() : 2);
    mine.LargeChange(10);
    mine.Margin(slider.Margin());
    mine.HorizontalAlignment(X::HorizontalAlignment::Stretch);
    mine.VerticalAlignment(X::VerticalAlignment::Center);
    C::Grid::SetColumn(mine, C::Grid::GetColumn(slider));
    C::Grid::SetRow(mine, C::Grid::GetRow(slider));
    C::Grid::SetColumnSpan(mine, C::Grid::GetColumnSpan(slider));
    {
      auto n = std::wstring(X::Automation::AutomationProperties::GetName(slider));
      X::Automation::AutomationProperties::SetName(
          mine, n.empty() ? sound::Text(L"Volume", L"Громкость", L"Volumen", L"Lautstärke", L"Volume", L"音量") : n.c_str());
    }
    mine.Value(slider.Value());
    hide(slider);
    insertAfter(slider, mine);
    quickSlider_ = mine;
    volume_ = mine;
    // Core Audio.
    quickVolume_ = std::make_unique<QuickVolume>();
    try {
      quickVolume_->Open([this](float value, bool muted) { ApplyVolume(value, muted, true); },
                         [this] { RefreshVolume(); });
    } catch (...) {
    }
    RefreshVolume();
    Keep(mine.ValueChanged(winrt::auto_revoke,
                           [this](auto &&, P::RangeBaseValueChangedEventArgs const &args) {
                             if (quickSync_ || !quickVolume_)
                               return;
                             quickUserAt_ = GetTickCount64();
                             auto value = float(args.NewValue() / 100);
                             try {
                               quickVolume_->SetValue(value);
                             } catch (...) {
                               Wh_Log(L"QuickVolumeSetError %08X", winrt::to_hresult());
                             }
                             QuickGlyph(value, value <= 0);
                           }));
    volumeTip_.Watch(view_, mine, journal_, revoke_);
    ListenVolumeWheel(
        mine, [this](auto &&, X::Input::PointerRoutedEventArgs const &args) {
          try {
            auto s = quickSlider_;
            if (!s || !s.IsEnabled())
              return;
            auto point = args.GetCurrentPoint(s);
            if (point.Properties().IsHorizontalMouseWheel())
              return;
            int delta = point.Properties().MouseWheelDelta();
            if (!delta)
              return;
            if (args.Handled()) {
              volumeTip_.Show(view_, s, s.Value());
              return;
            }
            args.Handled(true);
            s.Value(volumeWheel_.Next(s.Value(), s.Minimum(), s.Maximum(), delta,
                                      settings_.volumeWheelStep));
            volumeTip_.Show(view_, s, s.Value());
          } catch (...) {
            Wh_Log(L"QuickVolumeWheelError %08X", winrt::to_hresult());
          }
        }, revoke_);
    Keep(mute.Click(winrt::auto_revoke, [this](auto &&, auto &&) {
      quickUserAt_ = 0;
    }));
  }
  void Sliders(C::Grid const &l1) {
    auto group = Named(l1, L"SlidersGroup");
    if (!group)
      return;
    auto grid = Named(group, L"RootGridView").try_as<C::GridView>();
    if (!grid)
      return;
    if (First(grid, 2)) {
      // Top 2.4: the Settings plate 11.2 DIP below the divider, as the
      // Volume mixer heading on Sound. The bottom margin and the brightness row
      // height are corrected by AlignSteps for whole-pixel steps.
      Set(grid, X::FrameworkElement::MarginProperty(),
          winrt::box_value(X::Thickness{12, 2.4, 16, 11.4}));
      grid_ = grid;
      X::Style item(winrt::xaml_typename<C::GridViewItem>());
      if (auto base = grid.ItemContainerStyle())
        item.BasedOn(base);
      // Native slider items are ~46 DIP: -4/-4 gives the 38 DIP step from a header
      // row centre and between native items; the grid's bottom margin takes the 7
      // DIP back, so the distance from the panel edge stays.
      item.Setters().Append(X::Setter(X::FrameworkElement::MarginProperty(),
                                      winrt::box_value(X::Thickness{4, -4, 0, -4})));
      Set(grid, C::ItemsControl::ItemContainerStyleProperty(), item);
      Set(grid, C::ListViewBase::HeaderTransitionsProperty(), M::Animation::TransitionCollection());
      // The grid starts at 12 DIP so the Settings plate (from 12) is not
      // clipped by it; header and native items move back by 4.
      header_ = C::StackPanel();
      header_.Margin({4, 0, 0, 0});
      header_.Children().Append(SettingsRow());
      if (ddc_)
        header_.Children().Append(BrightnessRow());
      Set(grid, C::ListViewBase::HeaderProperty(), header_);
    }
    // The header presenter left-aligns its content by default.
    if (header_ && header_.IsLoaded())
      for (auto p = Parent(header_); p && p != grid; p = Parent(p))
        if (auto presenter = p.try_as<C::ContentControl>()) {
          if (First(presenter, 3))
            Set(presenter, C::Control::HorizontalContentAlignmentProperty(),
                winrt::box_value(X::HorizontalAlignment::Stretch));
          break;
        }
    auto l2 = Named(grid, L"VolumeL2Button").try_as<C::Button>();
    if (!l2)
      return;
    if (First(l2, 4)) {
      SmallButton(l2);
      quickMixerButton_ = l2;
      originalMixerIcon_ = l2.Content();
      journal_.Save(l2, C::ContentControl::ContentProperty());
      if (settings_.altQuickMixer)
        l2.Content(Icon(L"sliders-horizontal"));
      auto name = winrt::box_value(sound::Text(L"Sound", L"Звук", L"Sonido", L"Sound", L"Som", L"声音"));
      Set(l2, X::Automation::AutomationProperties::NameProperty(), name);
      Set(l2, C::ToolTipService::ToolTipProperty(), name);
      if (auto row = Parent(l2).try_as<C::Panel>())
        for (auto const &child : row.Children())
          if (auto toggle = child.try_as<P::ToggleButton>())
            SmallButton(toggle);
      // Hiding Quick Settings on the way to Sound (ExitWithSnapshot).
      if (auto original = l2.Command()) {
        auto weakView = winrt::make_weak(view_);
        Set(l2, P::ButtonBase::CommandProperty(),
            winrt::make<DelayedCommand>(original, [weakView, original](IInspectable const &p) {
              auto view = weakView.get();
              X::FrameworkElement l1{nullptr};
              if (view)
                l1 = Named(view, L"L1Grid");
              if (!l1) {
                original.Execute(p);
                return;
              }
              auto media = FindMedia(view);
              // The media widget above fades in parallel; see HoldMedia.
              bool fading = media && media.Visibility() == X::Visibility::Visible &&
                            media.ActualHeight() > 1 && media.Opacity() > 0 &&
                            winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled();
              if (fading) {
                FadeOutNow(media, 90);
                HoldMedia(media, 90);
              }
              revealAt = GetTickCount64();
              Leave(l1, l1, original, p);
            }));
      }
    }
    VolumeIcon(l2);
    // Track starts 12 DIP after the 32 DIP button, as on Sound: 40 DIP column + 8.
    if (auto row = Parent(l2).try_as<C::Panel>())
      for (auto const &child : row.Children())
        if (auto slider = child.try_as<C::Slider>();
            slider && !thinSliders_.count(winrt::get_abi(slider))) {
          auto look = Look();
          look.Geometry(slider);
          look.Thin(slider, [](double half) { return X::Thickness{8 - half, 0, 8 - half, 0}; });
        }
    if (quickSlider_)
      volume_ = quickSlider_;
    else if (auto row = Parent(l2).try_as<C::Panel>())
      for (auto const &child : row.Children())
        if (auto slider = child.try_as<C::Slider>())
          volume_ = slider;
  }
  // The frame grows smoothly on the way to Sound without changing the shell's
  // timings, heights or animations: when ControlCenterRegion gets Sound's
  // height, its visual is clipped to the old height at the bottom and the
  // rounded clip opens upwards in 220 ms (composition, the VM curve). Only after
  // the Sound button was pressed (1.5 s), so VM expansion is unaffected. The
  // shrink back cannot be done this way: a clip cannot show more than the layout.
  static inline ULONGLONG revealAt = 0;
  winrt::Windows::UI::Composition::CompositionRoundedRectangleGeometry revealGeometry_{nullptr};
  winrt::Windows::UI::Composition::CompositionRectangleGeometry edgeGeometry_{nullptr};
  winrt::weak_ref<X::FrameworkElement> edgeOwner_, revealRegion_;
  void FrameReveal() {
    X::FrameworkElement region{nullptr};
    for (auto e = Parent(view_); e; e = Parent(e))
      if (e.Name() == L"ControlCenterRegion") {
        region = e;
        break;
      }
    if (!region || !First(region, 11))
      return;
    auto weak = winrt::make_weak(region);
    auto alive = std::make_shared<bool>(true);
    Keep(region.SizeChanged(winrt::auto_revoke, [this, weak,
                                                 alive](auto &&, X::SizeChangedEventArgs const &a) {
      try {
        auto r = weak.get();
        if (!r)
          return;
        double h0 = a.PreviousSize().Height, h1 = a.NewSize().Height;
        float w = a.NewSize().Width;
        namespace Comp = winrt::Windows::UI::Composition;
        using winrt::Windows::Foundation::Numerics::float2;
        auto v = X::Hosting::ElementCompositionPreview::GetElementVisual(r);
        auto c = v.Compositor();
        // The panel's ThemeShadow follows the layout bounds, not the clip. It stays,
        // and the parent of its owner gets a plain clip whose top edge follows the
        // frame's top edge (same curve), wide and deep enough for the shadow on the
        // sides and below, so only the shadow above the growing edge is cut.
        auto shadowParent = [&]() -> X::FrameworkElement {
          for (X::FrameworkElement e = r; e; e = Parent(e))
            if (e.Shadow())
              return Parent(e);
          return Parent(r);
        };
        auto regionTop = [&](X::FrameworkElement const &p) -> float {
          try {
            return float(r.TransformToVisual(p).TransformPoint({0, 0}).Y);
          } catch (...) {
            return 0.f;
          }
        };
        if (revealGeometry_) {
          // With VM collapsed the height changes twice (the shell's L2 minimum
          // first, then the mod's smaller height): any size change keeps the visible
          // bottom and retargets the reveal.
          revealGeometry_.StopAnimation(L"Offset");
          revealGeometry_.StopAnimation(L"Size");
          auto o = revealGeometry_.Offset();
          auto z = revealGeometry_.Size();
          float top = std::max(0.f, o.y + float(h1 - h0));
          revealGeometry_.Offset({0, top});
          revealGeometry_.Size({w, std::max(0.f, std::min(z.y, float(h1) - top))});
          if (edgeGeometry_)
            if (auto p = edgeOwner_.get()) {
              edgeGeometry_.StopAnimation(L"Offset");
              edgeGeometry_.Offset({-64.f, regionTop(p) + top});
            }
        } else {
          if (!revealAt || GetTickCount64() - revealAt > 1500 || h0 <= 0 || h1 - h0 < 1 ||
              !winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled())
            return;
          revealAt = 0;
          revealGeometry_ = c.CreateRoundedRectangleGeometry();
          float radius = 8;
          if (auto g = r.try_as<C::Grid>())
            if (g.CornerRadius().TopLeft > 0)
              radius = float(g.CornerRadius().TopLeft);
          revealGeometry_.CornerRadius({radius, radius});
          revealGeometry_.Offset({0, float(h1 - h0)});
          revealGeometry_.Size({w, float(h0)});
          v.Clip(c.CreateGeometricClip(revealGeometry_));
          if (auto p = shadowParent()) {
            auto pv = X::Hosting::ElementCompositionPreview::GetElementVisual(p);
            edgeGeometry_ = c.CreateRectangleGeometry();
            edgeGeometry_.Offset({-64.f, regionTop(p) + float(h1 - h0)});
            edgeGeometry_.Size({float(p.ActualWidth()) + 128.f, float(p.ActualHeight()) + 4000.f});
            pv.Clip(c.CreateGeometricClip(edgeGeometry_));
            edgeOwner_ = winrt::make_weak(p);
          }
        }
        auto ease = c.CreateCubicBezierEasingFunction(float2{.1f, .9f}, float2{.2f, 1.f});
        auto duration = std::chrono::milliseconds(220);
        auto batch = c.CreateScopedBatch(Comp::CompositionBatchTypes::Animation);
        auto offset = c.CreateVector2KeyFrameAnimation();
        offset.InsertKeyFrame(1, float2{0, 0}, ease);
        offset.Duration(duration);
        auto size = c.CreateVector2KeyFrameAnimation();
        size.InsertKeyFrame(1, float2{w, float(h1)}, ease);
        size.Duration(duration);
        revealGeometry_.StartAnimation(L"Offset", offset);
        revealGeometry_.StartAnimation(L"Size", size);
        if (edgeGeometry_)
          if (auto p = edgeOwner_.get()) {
            auto edge = c.CreateVector2KeyFrameAnimation();
            edge.InsertKeyFrame(1, float2{-64.f, regionTop(p)}, ease);
            edge.Duration(duration);
            edgeGeometry_.StartAnimation(L"Offset", edge);
          }
        batch.End();
        auto geometry = revealGeometry_;
        batch.Completed([this, geometry, alive](auto &&, auto &&) {
          if (!*alive || revealGeometry_ != geometry)
            return;
          EndReveal();
        });
      } catch (...) {
        Wh_Log(L"FrameRevealError %08X", winrt::to_hresult());
      }
    }));
    revealRegion_ = weak;
    revoke_.push_back([this, alive] {
      *alive = false;
      EndReveal();
    });
  }
  void EndReveal() {
    revealGeometry_ = nullptr;
    edgeGeometry_ = nullptr;
    if (auto r = revealRegion_.get())
      try {
        X::Hosting::ElementCompositionPreview::GetElementVisual(r).Clip(nullptr);
      } catch (...) {
      }
    if (auto p = edgeOwner_.get())
      try {
        X::Hosting::ElementCompositionPreview::GetElementVisual(p).Clip(nullptr);
      } catch (...) {
      }
    edgeOwner_ = nullptr;
  }
  void Ensure() {
    if (restoring_ || !view_)
      return;
    dirty_ = false;
    lastEnsure_ = GetTickCount64();
    try {
      auto l1 = Named(view_, L"L1Grid").try_as<C::Grid>();
      if (!l1)
        return;
      RefreshVolume();
      // Back from Sound: L1Grid visible again; the transparency set for the
      // hiding picture (ExitWithSnapshot) and the mute glyph state are refreshed.
      if (First(l1, 9)) {
        auto weak = winrt::make_weak(l1);
        auto token = l1.RegisterPropertyChangedCallback(
            X::UIElement::VisibilityProperty(), [this, weak](auto &&, auto &&) {
              try {
                auto g = weak.get();
                if (!g || g.Visibility() != X::Visibility::Visible)
                  return;
                RefreshVolume();
                if (g.Opacity() == 0)
                  g.Opacity(1);
                if (auto media = FindMedia(view_)) {
                  if (media.Opacity() == 0)
                    media.Opacity(1);
                  ShowAgain(media);
                }
                for (auto const &child : g.Children())
                  if (child.Opacity() == 0)
                    child.Opacity(1);
                ShowAgain(g);
              } catch (...) {
              }
            });
        revoke_.push_back([weak, token] {
          if (auto g = weak.get())
            try {
              g.UnregisterPropertyChangedCallback(X::UIElement::VisibilityProperty(), token);
            } catch (...) {
            }
        });
      }
      DarkBlock(l1);
      TileRows(l1);
      Pager();
      Sliders(l1);
      PlaceDisplayActions();
      Footer();
      FrameReveal();
      if (bright_ && bright_.IsLoaded() && !thinSliders_.count(winrt::get_abi(bright_))) {
        auto look = Look();
        look.Geometry(bright_);
        look.Thin(bright_, [](double half) { return X::Thickness{8 - half, 0, 8 - half, 0}; });
      }
    } catch (...) {
      Wh_Log(L"Quick Settings layout: %08X", winrt::to_hresult());
    }
  }

public:
  QuickPage(X::FrameworkElement const &view, HWND host)
      : view_(view), host_(host), settings_(SettingsSnapshot()) {
    if (settings_.brightnessEnabled) {
      try {
        ddc_ = std::make_unique<Ddc>();
      } catch (...) {
        Wh_Log(L"QuickDdcError %08X", winrt::to_hresult());
      }
    }
    // Before the first frame of every opening: new templates appear while the
    // panel is laid out (Track marks the page dirty); LayoutUpdated precedes render.
    // Not applied here: the constructor runs inside the tree-change callback.
    // Opening: re-read the monitor brightness once the panel becomes visible.
    // New templates are not announced without tree reports: Ensure also runs
    // every 250 ms of layout activity (opening, paging, page changes).
    Keep(view_.LayoutUpdated(winrt::auto_revoke, [this](auto &&, auto &&) {
      if (dirty_ || GetTickCount64() - lastEnsure_ > 250)
        Ensure();
      try {
        AlignSteps();
      } catch (...) {
      }
      bool shown = PanelVisible();
      if (shown && !visible_) {
        visible_ = true;
        try {
          PollNight();
          Brightness(true);
        } catch (...) {
        }
      } else
        visible_ = shown;
    }));
  }
  X::FrameworkElement View() const { return view_; }
  void Dirty() { dirty_ = true; }
  bool ApplySettings(ModSettings const &next) {
    if (settings_.brightnessEnabled != next.brightnessEnabled ||
        settings_.quickActionRows != next.quickActionRows ||
        settings_.quickShortcuts != next.quickShortcuts)
      return false;
    auto previous = std::exchange(settings_, next);
    if (quickMixerButton_ && previous.altQuickMixer != next.altQuickMixer)
      quickMixerButton_.Content(next.altQuickMixer ? IInspectable(Icon(L"sliders-horizontal"))
                                                 : originalMixerIcon_);
    if (quickMute_ && previous.altQuickVolume != next.altQuickVolume) {
      quickIcon_.reset();
      quickGlyph_ = -2;
      if (next.altQuickVolume)
        try {
          quickIcon_ = std::make_shared<AnimatedVolume>();
          quickMute_.Content(quickIcon_->Isolated());
        } catch (...) {
          quickIcon_.reset();
          Wh_Log(L"QuickVolumeIconError %08X", winrt::to_hresult());
        }
      RefreshVolume();
    }
    return true;
  }
  void Refresh() {
    visible_ = PanelVisible();
    if (dirty_ || visible_)
      Ensure();
    try {
      ContinueNightSearch();
      PollNight();
      Brightness(false);
    } catch (...) {
    }
  }
  void Restore() {
    if (restoring_)
      return;
    restoring_ = true;
    volumeTip_.Hide();
    try {
      FinishNightSearch();
    } catch (...) {
      Wh_Log(L"QuickNightPagerRestoreError %08X", winrt::to_hresult());
    }
    if (quickVolume_) {
      quickVolume_->Close();
      quickVolume_.reset();
    }
    quickSlider_ = nullptr;
    quickMute_ = nullptr;
    quickIcon_ = nullptr;
    ddc_.reset();
    for (auto &r : revoke_)
      try {
        r();
      } catch (...) {
      }
    revoke_.clear();
    unavailableBrightness_.Restore();
    journal_.Restore();
    view_ = nullptr;
  }
  ~QuickPage() { Restore(); }
};
} // namespace native

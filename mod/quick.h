#pragma once
#include "common.h"
#include "volume-icon.h"
#include "slider-look.h"
#include "exit.h"
#include "quick-volume.h"
#include "ddc.h"
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
namespace native {
// Quick Settings main page (ControlCenter.ControlCenterView). L1Grid holds a
// light Border spanning TogglesGroup + SlidersGroup, then FooterGrid
// (LeftFooter: battery on laptops; RightFooter: the native All settings
// button). Changes:
// - the light Border covers the tiles only; sliders sit in the dark block;
// - All settings: title on the left (plate from 12, text from 20 DIP, like the
//   Volume mixer heading), gear in the right column;
// - dark block rows one 38 DIP step apart (StepPx): All settings,
//   [brightness], volume; 32 DIP buttons, 20 DIP side insets, thin sliders;
// - the native footer row is hidden when it has nothing but the gear;
// - the volume L2 button shows the mixer glyph without the chevron and still
//   runs the native LaunchAdvancedPageCommand;
// - the pager keeps its native place, without arrows, with plain dots.
// Native tiles, paging, commands, bindings and values are untouched; every
// change is journaled and restored on unload.
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
  C::Grid settingsRow_{nullptr};
  C::Button gear_{nullptr}, title_{nullptr};
  U::AnimatedIcon gearIcon_{nullptr};
  winrt::weak_ref<C::Button> nativeSettings_;
  // Brightness row: night light status/toggle, DDC/CI slider, display settings.
  HWND host_ = nullptr;
  std::unique_ptr<Ddc> ddc_;
  C::StackPanel header_{nullptr};
  C::Grid brightRow_{nullptr};
  C::Button night_{nullptr}, display_{nullptr};
  C::Slider bright_{nullptr};
  U::AnimatedIcon nightIcon_{nullptr};
  U::IAnimatedIconStatics statics_{nullptr};
  bool reapplying_ = false;
  std::vector<std::shared_ptr<AnimatedVolume>> volumeIcons_;
  std::wstring nightState_, nightTip_;
  int nightRegistry_ = -1;
  U::AnimatedVisualPlayer brightPlayer_{nullptr};
  U::IAnimatedVisualSource2 brightSource_{nullptr};
  X::FrameworkElement brightVisual_{nullptr};
  double brightFrom_ = 0, brightTo_ = 1;
  HMONITOR monitor_ = nullptr;
  unsigned seenRevision_ = 0;
  ULONGLONG lastRead_ = 0, lastUser_ = 0;
  bool sync_ = false, visible_ = false;
  X::DispatcherTimer brightIdle_{nullptr};
  winrt::event_token brightIdleTick_{};
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
  C::Slider quickSlider_{nullptr};
  C::Button quickMute_{nullptr};
  std::shared_ptr<AnimatedVolume> quickIcon_;
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
    if (quickMute_ && quickMute_.IsEnabled() != available)
      quickMute_.IsEnabled(available);
    if (quickIcon_) {
      if (dragging) {
        auto v = float(quickSlider_.Value() / 100);
        quickIcon_->Update(v, v <= 0, available);
      } else
        quickIcon_->Update(value, muted, available);
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
  std::wstring SettingsName() {
    if (auto b = nativeSettings_.get()) {
      auto n = std::wstring(X::Automation::AutomationProperties::GetName(b));
      if (!n.empty())
        return n;
    }
    return L"All settings";
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
  // Gear (native QA_All_Settings animation, FooterIconButtonStyle states) and an
  // "All settings" title styled like the Volume mixer title; both run the native
  // footer button's command, which opens Settings and closes the panel.
  C::Grid SettingsRow() {
    // 38 DIP steps: 5.6 + 38.4 lands on whole pixels at 125% (7 + 48 px).
    auto row = Grid(L"40,*,40");
    row.Height(38.4);
    row.Margin({0, 5.6, 0, 0});
    auto name = SettingsName();
    gear_ = C::Button();
    if (auto style = Resource(view_, L"FooterIconButtonStyle").try_as<X::Style>())
      gear_.Style(style);
    gear_.Width(36);
    gear_.Height(36);
    gear_.MinWidth(0);
    gear_.MinHeight(0);
    gear_.Padding({0, 0, 0, 0});
    gear_.HorizontalAlignment(X::HorizontalAlignment::Center);
    gear_.VerticalAlignment(X::VerticalAlignment::Center);
    X::Automation::AutomationProperties::SetName(gear_, name);
    C::ToolTipService::SetToolTip(gear_, winrt::box_value(name));
    try {
      auto factory = NativeAnimatedFactory();
      IInspectable inner{nullptr};
      gearIcon_ = factory.as<U::IAnimatedIconFactory>().CreateInstance(nullptr, inner);
      auto statics = factory.as<U::IAnimatedIconStatics>();
      gearIcon_.Source(NativeSource(L"ControlCenter.QA_All_Settings"));
      gearIcon_.Width(16);
      gearIcon_.Height(16);
      gearIcon_.IsHitTestVisible(false);
      gear_.Content(gearIcon_);
      // The native footer icon binds its State to the button's AnimatedIcon.State.
      auto icon = gearIcon_;
      auto gear = gear_;
      auto token = gear_.RegisterPropertyChangedCallback(
          statics.StateProperty(),
          [statics, icon](X::DependencyObject const &sender, X::DependencyProperty const &) {
            try {
              statics.SetState(icon, statics.GetState(sender));
            } catch (...) {
            }
          });
      revoke_.push_back([gear, statics, token] {
        gear.UnregisterPropertyChangedCallback(statics.StateProperty(), token);
      });
    } catch (...) {
      Wh_Log(L"QuickGearIconError %08X", winrt::to_hresult());
      gear_.Content(SymbolGlyph(view_, 0xe713, 16));
    }
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
    title_.Content(text);
    X::Automation::AutomationProperties::SetName(title_, name);
    // Title on the left like the section headings, the gear in the right column
    // (with the display and sound settings buttons).
    C::Grid::SetColumn(title_, 0);
    C::Grid::SetColumnSpan(title_, 2);
    row.Children().Append(title_);
    C::Grid::SetColumn(gear_, 2);
    row.Children().Append(gear_);
    Keep(gear_.Click(winrt::auto_revoke, [this](auto &&, auto &&) { OpenSettings(); }));
    Keep(title_.Click(winrt::auto_revoke, [this](auto &&, auto &&) { OpenSettings(); }));
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
  // Brightness row: night light (status and toggle), brightness over DDC/CI
  // where Windows has no brightness slider, display settings.
  C::Grid BrightnessRow() {
    // Header rows on whole pixels at 125%: rows of fractional pixel height push
    // every row below off the grid.
    auto row = Grid(L"40,*,40");
    row.Height(39.2);
    row.Visibility(X::Visibility::Collapsed); // corrected by AlignSteps
    night_ = IconButton(L"Night light");
    // A plain Grid parent: the icons take only the states set here (see AnimatedVolume).
    C::Grid icons;
    icons.IsHitTestVisible(false);
    icons.HorizontalAlignment(X::HorizontalAlignment::Center);
    icons.VerticalAlignment(X::VerticalAlignment::Center);
    // QA_Nightlight draws its picture smaller in the 16 DIP box than
    // QA_Brightness (at 125%: 18 px vs 20 px wide); 17.6 DIP (22 px box) matches
    // the brightness picture that replaces it while the slider moves.
    try {
      nightIcon_ = NativeIcon(L"ControlCenter.QA_Nightlight");
      nightIcon_.Width(17.6);
      nightIcon_.Height(17.6);
      icons.Children().Append(nightIcon_);
    } catch (...) {
      Wh_Log(L"QuickBrightnessIconError %08X", winrt::to_hresult());
      icons.Children().Append(SymbolGlyph(view_, 0xe706, 16));
    }
    try {
      BrightnessIcon();
      if (brightVisual_) {
        brightVisual_.Visibility(X::Visibility::Collapsed);
        icons.Children().Append(brightVisual_);
      }
    } catch (...) {
      Wh_Log(L"QuickBrightnessPlayerError %08X", winrt::to_hresult());
    }
    night_.Content(icons);
    row.Children().Append(night_);
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
    X::Automation::AutomationProperties::SetName(bright_, L"Brightness");
    C::Grid::SetColumn(bright_, 1);
    row.Children().Append(bright_);
    display_ = IconButton(L"Display settings");
    display_.Content(SymbolGlyph(view_, 0xe7f4, 13.6));
    C::Grid::SetColumn(display_, 2);
    row.Children().Append(display_);
    Keep(night_.Click(winrt::auto_revoke, [this](auto &&, auto &&) { ToggleNight(); }));
    // 13.6 DIP (17 px at 125%), the size of the mixer and sound settings glyphs
    // in the same column.
    Keep(display_.Click(winrt::auto_revoke, [](auto &&, auto &&) {
      ShellExecuteW(nullptr, L"open", L"ms-settings:display", nullptr, nullptr, SW_SHOWNORMAL);
    }));
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
    brightIdle_ = X::DispatcherTimer();
    brightIdle_.Interval(std::chrono::milliseconds(800));
    brightIdleTick_ = brightIdle_.Tick([this](auto &&, auto &&) {
      brightIdle_.Stop();
      if (brightVisual_ && nightIcon_) {
        brightVisual_.Visibility(X::Visibility::Collapsed);
        nightIcon_.Visibility(X::Visibility::Visible);
      }
    });
    // The status is read before the first state is set; with the -1 default the
    // icon would start as NormalOff and animate to On a moment later.
    nightRegistry_ = RegistryNight();
    brightRow_ = row;
    NightState();
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
    brightVisual_ = brightPlayer_;
  }
  // While brightness changes the button shows the native brightness icon at the
  // new level, then the night light status 800 ms after the last change.
  void ShowBrightness(int value) {
    if (!brightVisual_ || !nightIcon_)
      return;
    try {
      if (brightSource_) {
        bool light = view_.ActualTheme() == X::ElementTheme::Light;
        brightSource_.SetColorProperty(L"Foreground",
                                       light ? winrt::Windows::UI::Color{0xE4, 0, 0, 0}
                                             : winrt::Windows::UI::Color{0xFF, 0xFF, 0xFF, 0xFF});
      }
      if (brightPlayer_)
        brightPlayer_.SetProgress(brightFrom_ +
                                  (brightTo_ - brightFrom_) * std::clamp(value, 0, 100) / 100.);
    } catch (...) {
    }
    nightIcon_.Visibility(X::Visibility::Collapsed);
    brightVisual_.Visibility(X::Visibility::Visible);
    brightIdle_.Stop();
    brightIdle_.Start();
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
    std::wstring name = std::wstring(L"Night light") + (on ? L": on" : L": off");
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
  // The native tile when it is realized (page 2 tiles are released when page 1
  // shows, and containers are recycled, so it is looked up on every click).
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
      if (Class(p).find(L"ToggleButton") != std::wstring::npos)
        return p;
    return nullptr;
  }
  // Night light toggle: the native tile through UI Automation when it is
  // realized, otherwise the Night light page of Settings.
  void ToggleNight() {
    try {
      if (auto tile = NightTileNow())
        if (auto peer =
                X::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(tile)) {
          if (auto toggle = peer.GetPattern(X::Automation::Peers::PatternInterface::Toggle)
                                .try_as<X::Automation::Provider::IToggleProvider>()) {
            toggle.Toggle();
            nightRegistry_ = nightRegistry_ == 1 ? 0 : 1;
            NightState();
            return;
          }
          if (auto invoke = peer.GetPattern(X::Automation::Peers::PatternInterface::Invoke)
                                .try_as<X::Automation::Provider::IInvokeProvider>()) {
            invoke.Invoke();
            nightRegistry_ = nightRegistry_ == 1 ? 0 : 1;
            NightState();
            return;
          }
        }
    } catch (...) {
      Wh_Log(L"QuickNightToggleError %08X", winrt::to_hresult());
    }
    ShellExecuteW(nullptr, L"open", L"ms-settings:nightlight", nullptr, nullptr, SW_SHOWNORMAL);
  }
  bool NativeBrightness() {
    auto group = Named(view_, L"SlidersGroup");
    return group && Named(group, L"BrightnessPlayer");
  }
  bool PanelVisible() { return WindowShown(host_); }
  void ApplyBright(int value) {
    if (!brightRow_)
      return;
    bool show = value >= 0 && !NativeBrightness();
    auto want = show ? X::Visibility::Visible : X::Visibility::Collapsed;
    if (brightRow_.Visibility() != want)
      brightRow_.Visibility(want);
    if (value >= 0 && std::lround(bright_.Value()) != value) {
      sync_ = true;
      bright_.Value(value);
      sync_ = false;
    }
  }
  // Panel monitor: cached value at once, a DDC/CI read on opening and every 3 s
  // while open (not while the user is changing it).
  void Brightness(bool opening) {
    if (!brightRow_ || !ddc_)
      return;
    auto now = GetTickCount64();
    auto monitor = MonitorFromWindow(host_, MONITOR_DEFAULTTONEAREST);
    if (monitor != monitor_) {
      monitor_ = monitor;
      lastRead_ = 0;
      auto cached = brightCache.find(monitor);
      ApplyBright(cached != brightCache.end() ? cached->second : -1);
    }
    if (!NativeBrightness() && (opening || now - lastRead_ > 3000) && now - lastUser_ > 1500 &&
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
          ApplyBright(value);
      }
    }
  }

  void Footer() {
    auto footer = Named(view_, L"FooterGrid");
    if (!footer)
      return;
    if (!nativeSettings_.get())
      if (auto icon = Find(footer, [](auto const &e) {
            auto a = e.template try_as<U::AnimatedIcon>();
            return a && Class(a.Source()) == L"ControlCenter.QA_All_Settings";
          }))
        for (auto p = Parent(icon); p && p != footer; p = Parent(p))
          if (auto button = p.try_as<C::Button>()) {
            nativeSettings_ = winrt::make_weak(button);
            break;
          }
    // LeftFooter shows battery etc. on laptops: keep the row then.
    auto left = Named(footer, L"LeftFooter");
    bool used = left && Find(left, [&](auto const &e) {
                  return e != left && e.template try_as<C::ItemsControl>() &&
                         e.Visibility() == X::Visibility::Visible;
                });
    auto want = used || !nativeSettings_.get() ? X::Visibility::Visible : X::Visibility::Collapsed;
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
    std::shared_ptr<AnimatedVolume> icon;
    try {
      icon = std::make_shared<AnimatedVolume>();
    } catch (...) {
      Wh_Log(L"QuickVolumeIconError %08X", winrt::to_hresult());
      return;
    }
    volumeIcons_.push_back(icon);
    quickIcon_ = icon;
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
    // Mute button, built as SV's on the Sound page (Button(), IconButtonStyle,
    // 32 DIP, the isolated system QA_Volume icon), in the toggle's Grid cell.
    auto own = Button(view_, L"Mute system volume");
    own.Margin(mute.Margin());
    own.HorizontalAlignment(mute.HorizontalAlignment());
    own.VerticalAlignment(X::VerticalAlignment::Center);
    C::Grid::SetColumn(own, C::Grid::GetColumn(mute));
    C::Grid::SetRow(own, C::Grid::GetRow(mute));
    own.Content(icon->Isolated());
    {
      uint32_t index = 0;
      if (row.Children().IndexOf(mute, index)) {
        row.Children().RemoveAt(index);
        row.Children().InsertAt(index, own);
      } else
        row.Children().Append(own);
      auto weakRow = winrt::make_weak(row);
      journal_.undo.push_back([weakRow, own, mute, index] {
        if (auto r = weakRow.get()) {
          uint32_t i = 0;
          if (r.Children().IndexOf(own, i))
            r.Children().RemoveAt(i);
          if (!r.Children().IndexOf(mute, i))
            r.Children().InsertAt(std::min(index, r.Children().Size()), mute);
        }
      });
    }
    quickMute_ = own;
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
    mine.IsThumbToolTipEnabled(true);
    mine.Margin(slider.Margin());
    mine.HorizontalAlignment(X::HorizontalAlignment::Stretch);
    mine.VerticalAlignment(X::VerticalAlignment::Center);
    C::Grid::SetColumn(mine, C::Grid::GetColumn(slider));
    C::Grid::SetRow(mine, C::Grid::GetRow(slider));
    C::Grid::SetColumnSpan(mine, C::Grid::GetColumnSpan(slider));
    {
      auto n = std::wstring(X::Automation::AutomationProperties::GetName(slider));
      X::Automation::AutomationProperties::SetName(mine, n.empty() ? L"Volume" : n.c_str());
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
                             if (quickIcon_)
                               quickIcon_->Update(value, value <= 0, quickAvailable_);
                           }));
    Keep(mine.PointerWheelChanged(
        winrt::auto_revoke, [this](auto &&, X::Input::PointerRoutedEventArgs const &args) {
          try {
            auto s = quickSlider_;
            if (!s || args.Handled() || !s.IsEnabled())
              return;
            auto point = args.GetCurrentPoint(s);
            if (point.Properties().IsHorizontalMouseWheel())
              return;
            double delta = point.Properties().MouseWheelDelta() / 120.;
            if (!delta)
              return;
            s.Value(std::clamp(s.Value() + delta * s.SmallChange(), s.Minimum(), s.Maximum()));
            args.Handled(true);
          } catch (...) {
          }
        }));
    Keep(own.Click(winrt::auto_revoke, [this](auto &&, auto &&) {
      try {
        if (quickVolume_)
          quickVolume_->ToggleMute();
      } catch (...) {
        Wh_Log(L"QuickMuteToggleError %08X", winrt::to_hresult());
      }
      quickUserAt_ = 0;
      RefreshVolume();
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
      // Top 2.4: the All settings plate 11.2 DIP below the divider, as the
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
      // The grid starts at 12 DIP so the All settings plate (from 12) is not
      // clipped by it; header and native items move back by 4.
      header_ = C::StackPanel();
      header_.Margin({4, 0, 0, 0});
      header_.Children().Append(SettingsRow());
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
      Set(l2, C::ContentControl::ContentProperty(), Icon(L"sliders-horizontal"));
      Set(l2, C::ToolTipService::ToolTipProperty(), winrt::box_value(L"Sound settings"));
      Set(l2, X::Automation::AutomationProperties::NameProperty(),
          winrt::box_value(L"Sound settings"));
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
      Footer();
      DarkBlock(l1);
      Pager();
      Sliders(l1);
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
  QuickPage(X::FrameworkElement const &view, HWND host) : view_(view), host_(host) {
    try {
      ddc_ = std::make_unique<Ddc>();
      monitor_ = MonitorFromWindow(host_, MONITOR_DEFAULTTOPRIMARY);
      ddc_->Read(monitor_);
    } catch (...) {
      Wh_Log(L"QuickDdcError %08X", winrt::to_hresult());
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
  void Refresh() {
    visible_ = PanelVisible();
    if (dirty_ || visible_)
      Ensure();
    try {
      PollNight();
      Brightness(false);
    } catch (...) {
    }
  }
  void Restore() {
    if (restoring_)
      return;
    restoring_ = true;
    if (quickVolume_) {
      quickVolume_->Close();
      quickVolume_.reset();
    }
    quickSlider_ = nullptr;
    quickMute_ = nullptr;
    quickIcon_ = nullptr;
    if (brightIdle_) {
      brightIdle_.Stop();
      brightIdle_.Tick(brightIdleTick_);
      brightIdle_ = nullptr;
    }
    ddc_.reset();
    for (auto &r : revoke_)
      try {
        r();
      } catch (...) {
      }
    revoke_.clear();
    journal_.Restore();
    view_ = nullptr;
  }
  ~QuickPage() { Restore(); }
};
} // namespace native

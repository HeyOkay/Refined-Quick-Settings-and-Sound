#pragma once
#include "common.h"
#include <winrt/Microsoft.UI.Xaml.Controls.h>
namespace native {
namespace U = winrt::Microsoft::UI::Xaml::Controls;
// Exact system Lottie asset: no SVG replacement or authored volume animation.
inline winrt::Windows::Foundation::IActivationFactory NativeFactory(PCWSTR type, HMODULE module) {
  using Get = HRESULT(WINAPI *)(void *, void **);
  auto get = reinterpret_cast<Get>(GetProcAddress(module, "DllGetActivationFactory"));
  if (!get)
    throw winrt::hresult_error(E_NOINTERFACE);
  winrt::Windows::Foundation::IActivationFactory factory{nullptr};
  winrt::hstring name(type);
  winrt::check_hresult(get(winrt::get_abi(name), winrt::put_abi(factory)));
  return factory;
}
inline U::IAnimatedVisualSource2 NativeSource(PCWSTR type) {
  auto module = GetModuleHandleW(L"ControlCenter.dll");
  // One process-lifetime system resource load, reused on later mod reloads.
  // Composition can retain the native visual after its XAML owner is removed.
  if (!module)
    module = LoadLibraryExW(L"ControlCenter.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!module)
    winrt::throw_last_error();
  return NativeFactory(type, module)
      .ActivateInstance<IInspectable>()
      .as<U::IAnimatedVisualSource2>();
}
inline U::IAnimatedVisualSource2 NativeVolumeSource() {
  return NativeSource(L"ControlCenter.QA_Volume");
}
inline winrt::Windows::Foundation::IActivationFactory NativeAnimatedFactory() {
  try {
    return winrt::get_activation_factory<U::AnimatedIcon>();
  } catch (...) {
    auto module = GetModuleHandleW(L"Microsoft.UI.Xaml.dll");
    if (!module)
      throw;
    return NativeFactory(L"Microsoft.UI.Xaml.Controls.AnimatedIcon", module);
  }
}
inline PCWSTR VolumeState(float value, bool muted) {
  return muted           ? L"Mute"
         : value <= 0    ? L"Volume_00"
         : value <= .33f ? L"Volume_01"
         : value <= .66f ? L"Volume_33"
                         : L"Volume_66";
}
// AnimatedIcon queues a state change that arrives during a transition and
// plays it after the current one, so quick presses of a volume button played
// the earlier animations in turn. A change during a transition starts fresh: a
// new AnimatedIcon (same system QA_Volume asset) is put into the host showing
// the previous target state, and on load it gets the new state, so only the
// latest transition plays; the old icon is removed then.
class AnimatedVolume {
  U::AnimatedIcon icon_{nullptr};
  C::Grid host_{nullptr};
  U::IAnimatedIconStatics statics_{nullptr};
  winrt::Windows::Foundation::IActivationFactory factory_{nullptr};
  std::wstring state_;
  int64_t stateToken_ = 0;
  bool reapplying_ = false;
  ULONGLONG changedAt_ = 0;
  winrt::event_token loadedToken_{};
  U::AnimatedIcon loading_{nullptr};
  static constexpr ULONGLONG transitionMs = 700; // QA_Volume transitions are shorter
  U::AnimatedIcon Make() {
    auto source = NativeVolumeSource();
    IInspectable inner{nullptr};
    auto icon = factory_.as<U::IAnimatedIconFactory>().CreateInstance(nullptr, inner);
    // 16 DIP, the size of the shell's own icons.
    icon.Source(source);
    icon.Width(16);
    icon.Height(16);
    icon.MirroredWhenRightToLeft(false);
    icon.HorizontalAlignment(X::HorizontalAlignment::Center);
    icon.VerticalAlignment(X::VerticalAlignment::Center);
    icon.IsHitTestVisible(false);
    if (!source.Markers().HasKey(L"MuteToVolume_01_End"))
      throw winrt::hresult_error(E_NOTIMPL, L"Unknown native volume markers");
    return icon;
  }
  // Hovering the SV button still switched the glyph to Mute with the Grid
  // isolation: whoever writes AnimatedIcon.State on the icon (QA_Volume has
  // no PointerOver/Normal states), the volume state set by Update() wins.
  void Watch() {
    if (icon_)
      stateToken_ = icon_.RegisterPropertyChangedCallback(
          statics_.StateProperty(),
          [this](X::DependencyObject const &, X::DependencyProperty const &) { Reapply(); });
  }
  void Unwatch() {
    if (icon_ && stateToken_)
      try {
        icon_.UnregisterPropertyChangedCallback(statics_.StateProperty(), stateToken_);
      } catch (...) {
      }
    stateToken_ = 0;
  }
  void SetOn(U::AnimatedIcon const &icon, std::wstring const &state) {
    reapplying_ = true;
    try {
      statics_.SetState(icon, state);
    } catch (...) {
      reapplying_ = false;
      throw;
    }
    reapplying_ = false;
  }
  void Fresh(std::wstring const &from) {
    auto next = Make();
    SetOn(next, from);
    if (loading_) {
      try {
        loading_.Loaded(loadedToken_);
      } catch (...) {
      }
    }
    Unwatch();
    auto old = icon_;
    icon_ = next;
    Watch();
    // The previous icon stays until the new one is loaded, so no frame is empty.
    auto weakOld = winrt::make_weak(old);
    auto weakHost = winrt::make_weak(host_);
    loading_ = next;
    loadedToken_ = next.Loaded([this, weakOld, weakHost](IInspectable const &sender, auto &&) {
      try {
        auto icon = sender.as<U::AnimatedIcon>();
        icon.Loaded(loadedToken_);
        if (loading_ == icon)
          loading_ = nullptr;
        if (icon == icon_ && !state_.empty())
          SetOn(icon, state_);
        if (auto h = weakHost.get())
          for (int32_t k = int32_t(h.Children().Size()) - 1; k >= 0; k--) {
            auto child = h.Children().GetAt(uint32_t(k)).try_as<U::AnimatedIcon>();
            if (!child || child != icon_)
              h.Children().RemoveAt(uint32_t(k));
          }
      } catch (...) {
      }
    });
    host_.Children().Append(next);
  }

public:
  AnimatedVolume() {
    factory_ = NativeAnimatedFactory();
    statics_ = factory_.as<U::IAnimatedIconStatics>();
    icon_ = Make();
    Watch();
    // AnimatedIcon also follows AnimatedIcon.State set on its parent: a button
    // template sets "PointerOver"/"Pressed" there. QA_Volume has no such states,
    // so hovering the SV button left the glyph in its Mute frame. A plain Grid as
    // the direct parent keeps the state set only by Update().
    host_ = C::Grid();
    host_.IsHitTestVisible(false);
    host_.HorizontalAlignment(X::HorizontalAlignment::Center);
    host_.VerticalAlignment(X::VerticalAlignment::Center);
    host_.Children().Append(icon_);
  }
  AnimatedVolume(AnimatedVolume const &) = delete;
  AnimatedVolume &operator=(AnimatedVolume const &) = delete;
  ~AnimatedVolume() {
    Unwatch();
    if (loading_)
      try {
        loading_.Loaded(loadedToken_);
      } catch (...) {
      }
  }
  void Reapply() {
    if (reapplying_ || state_.empty() || !icon_)
      return;
    try {
      auto now = std::wstring(statics_.GetState(icon_));
      if (now == state_)
        return;
      SetOn(icon_, state_);
    } catch (...) {
    }
  }
  // Always the isolating Grid (the icon inside it is replaced).
  X::FrameworkElement Element() { return host_; }
  X::FrameworkElement Isolated() { return host_; }
  void Update(float value, bool muted, bool available = true) {
    std::wstring state = available ? VolumeState(value, muted) : L"Disabled";
    if (state_ == state)
      return;
    auto previous = state_;
    state_ = state;
    auto now = GetTickCount64();
    bool playing = !previous.empty() && now - changedAt_ < transitionMs;
    changedAt_ = now;
    if (playing && host_ && host_.IsLoaded()) {
      try {
        Fresh(previous);
        return;
      } catch (...) {
        Wh_Log(L"VolumeIconFreshError %08X", winrt::to_hresult());
      }
    }
    if (loading_)
      return; // the loading icon takes state_ when it is loaded
    SetOn(icon_, state);
  }
};
} // namespace native

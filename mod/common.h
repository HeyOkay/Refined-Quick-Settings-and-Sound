#pragma once
#ifndef WH_WINRT_WINUI2
#define WH_WINRT_WINUI2
#endif
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <windhawk_utils.h>
#include <ocidl.h>
#include <mutex>
#include <set>
#include <map>
#include <memory>
#include <functional>
#include <atomic>
#undef GetCurrentTime
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include "audio.h"
#include "icons.h"
namespace native {
namespace X = winrt::Windows::UI::Xaml;
namespace C = X::Controls;
namespace P = C::Primitives;
namespace M = X::Media;
namespace D = X::Data;
using winrt::Windows::Foundation::IInspectable;
inline std::atomic<bool> stopping = false;
inline HMODULE module = nullptr;
inline std::vector<std::wstring> hidden;
// Post only an integer message, never a callback or object pointer. Removing
// the subclass on unload makes any remaining queued notifications harmless.
inline UINT volumePulseMessage = RegisterWindowMessageW(L"Windhawk.NativeSound.VolumePulse.1");
struct VolumePulse {
  HWND window = nullptr;
  std::atomic<bool> active{true}, queued{false};
  explicit VolumePulse(HWND value) : window(value) {}
  void Post() {
    if (active && !queued.exchange(true))
      if (!window || !PostMessageW(window, volumePulseMessage, 0, 0))
        queued = false;
  }
};
inline X::FrameworkElement Icon(PCWSTR name, double size = 17, double stroke = 1.6) {
  std::wstring geometry = sound::IconMarkup(name);
  for (size_t at = 0; (at = geometry.find(L"StrokeThickness=\"2\"", at)) != geometry.npos;) {
    auto value = L"StrokeThickness=\"" + std::to_wstring(stroke) + L"\"";
    geometry.replace(at, 19, value);
    at += value.size();
  }
  auto icon =
      X::Markup::XamlReader::Load(
          L"<Viewbox xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' Width='" +
          std::to_wstring(size) + L"' Height='" + std::to_wstring(size) +
          L"' HorizontalAlignment='Center' VerticalAlignment='Center' "
          L"UseLayoutRounding='True'><Canvas Width='24' Height='24'>" +
          geometry + L"</Canvas></Viewbox>")
          .as<X::FrameworkElement>();
  return icon;
}
template <class F> X::FrameworkElement Find(X::DependencyObject const &root, F const &match) {
  if (!root)
    return nullptr;
  if (auto e = root.try_as<X::FrameworkElement>(); e && match(e))
    return e;
  for (int i = 0; i < M::VisualTreeHelper::GetChildrenCount(root); i++)
    if (auto e = Find(M::VisualTreeHelper::GetChild(root, i), match))
      return e;
  return nullptr;
}
inline X::FrameworkElement Named(X::DependencyObject const &root, PCWSTR name) {
  return Find(root, [&](auto const &e) { return e.Name() == name; });
}
inline X::FrameworkElement Parent(X::DependencyObject const &e) {
  return M::VisualTreeHelper::GetParent(e).try_as<X::FrameworkElement>();
}
inline std::wstring Class(IInspectable const &e) {
  return e ? std::wstring(winrt::get_class_name(e)) : L"";
}
inline IInspectable VolumeModel(C::Slider const &slider) {
  if (auto expression = slider.GetBindingExpression(P::RangeBase::ValueProperty())) {
    if (auto data = expression.DataItem())
      return data;
    if (auto source = expression.ParentBinding().Source())
      return source;
  }
  return slider.DataContext();
}
inline IInspectable Resource(X::FrameworkElement root, PCWSTR name) {
  auto key = winrt::box_value(name);
  for (auto e = root; e; e = Parent(e))
    if (e.Resources().HasKey(key))
      return e.Resources().Lookup(key);
  auto app = X::Application::Current();
  if (app && app.Resources().HasKey(key))
    return app.Resources().Lookup(key);
  return nullptr;
}
// Use the shell's symbol font, with the same volume glyphs it uses in the tray.
// No SVG substitute or suppression of the native taskbar glyph is needed.
inline C::FontIcon SymbolGlyph(X::FrameworkElement const &owner, wchar_t code, double size = 16) {
  C::FontIcon icon;
  if (auto font = Resource(owner, L"SymbolThemeFontFamily").try_as<M::FontFamily>())
    icon.FontFamily(font);
  icon.Glyph(winrt::hstring(std::wstring(1, code)));
  icon.FontSize(size);
  icon.Width(size);
  icon.Height(size);
  icon.HorizontalAlignment(X::HorizontalAlignment::Center);
  icon.VerticalAlignment(X::VerticalAlignment::Center);
  icon.IsHitTestVisible(false);
  return icon;
}
inline C::FontIcon VolumeGlyph(X::FrameworkElement const &owner, int level, double size = 16) {
  return SymbolGlyph(owner,
                     level == 0   ? 0xe74f
                     : level == 1 ? 0xe993
                     : level == 2 ? 0xe994
                                  : 0xe995,
                     size);
}
// Built in code: XamlReader::Load ran several times per Sound opening (pickers,
// input rows, master row), and parsed XAML stays registered with XAML Diagnostics.
inline C::Grid Grid(PCWSTR columns) {
  C::Grid grid;
  std::wstring s(columns);
  for (size_t a = 0; a < s.size();) {
    auto b = s.find(',', a);
    if (b == s.npos)
      b = s.size();
    auto part = s.substr(a, b - a);
    C::ColumnDefinition column;
    if (part == L"*")
      column.Width(X::GridLengthHelper::FromValueAndType(1, X::GridUnitType::Star));
    else if (part == L"Auto")
      column.Width(X::GridLengthHelper::Auto());
    else
      column.Width(X::GridLengthHelper::FromValueAndType(std::stod(part), X::GridUnitType::Pixel));
    grid.ColumnDefinitions().Append(column);
    a = b + 1;
  }
  return grid;
}
inline C::Button Button(X::FrameworkElement const &owner, PCWSTR name, PCWSTR glyph = nullptr) {
  C::Button b;
  b.Width(32);
  b.Height(32);
  b.MinWidth(0);
  b.MinHeight(0);
  b.Padding({0, 0, 0, 0});
  if (auto style = Resource(owner, L"IconButtonStyle").try_as<X::Style>())
    b.Style(style);
  X::Automation::AutomationProperties::SetName(b, name);
  C::ToolTipService::SetToolTip(b, winrt::box_value(name));
  if (glyph)
    b.Content(Icon(glyph));
  return b;
}
// Block divider colour: the Quick Settings tiles Border, whose bottom edge
// divides the tiles from the All settings block. Set by QuickPage, read by the
// Sound page; a binding follows theme changes.
inline winrt::weak_ref<C::Border> quickDivider;
inline C::Border QuickDivider() {
  auto b = quickDivider.get();
  if (!b)
    return nullptr;
  auto t = b.BorderThickness();
  return t.Bottom > 0 || t.Top > 0 ? b : nullptr;
}
inline D::Binding DividerBinding(C::Border const &source) {
  D::Binding binding;
  binding.Source(source);
  binding.Path(X::PropertyPath(L"BorderBrush"));
  binding.Mode(D::BindingMode::OneWay);
  return binding;
}
// Dark theme: every divider and slider rail looks like the line above All
// settings. An opaque grey would look wrong over a tinted wallpaper.
// Shown and not cloaked by DWM (the shell hides its panel windows by cloaking).
inline bool WindowShown(HWND window) {
  if (!window || !IsWindowVisible(window))
    return false;
  using Dwm = HRESULT(WINAPI *)(HWND, DWORD, void *, DWORD);
  static auto get = [] {
    auto dll = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return dll ? reinterpret_cast<Dwm>(GetProcAddress(dll, "DwmGetWindowAttribute")) : nullptr;
  }();
  DWORD cloaked = 0;
  return !(get && SUCCEEDED(get(window, 14 /*DWMWA_CLOAKED*/, &cloaked, sizeof(cloaked))) &&
           cloaked);
}
inline bool DarkTheme(X::FrameworkElement const &e) {
  return e && e.ActualTheme() == X::ElementTheme::Dark;
}
// That line is the system DividerStrokeColorDefault (#15FFFFFF, white at 8.3%)
// over the tiles layer (#09FFFFFF).
inline M::Brush DarkDivider() {
  return M::SolidColorBrush(winrt::Windows::UI::Color{0x15, 0xFF, 0xFF, 0xFF});
}
// Lines drawn directly on the base acrylic (Quick Settings dark block, Sound
// footer: SV divider and rails) have no layer under them, so they use the
// composition of both: 1-(1-0.035)(1-0.082) = 0.114 (#1DFFFFFF).
inline M::Brush DarkBaseDivider() {
  return M::SolidColorBrush(winrt::Windows::UI::Color{0x1D, 0xFF, 0xFF, 0xFF});
}
// Light theme: the Quick Settings Border's stroke, or the system divider
// stroke before that Border is known.
inline void PaintDivider(X::Shapes::Shape const &shape, X::FrameworkElement const &owner,
                         bool onBase = true) {
  if (DarkTheme(owner)) {
    shape.Fill(onBase ? DarkBaseDivider() : DarkDivider());
    return;
  }
  if (auto source = QuickDivider()) {
    shape.SetBinding(X::Shapes::Shape::FillProperty(), DividerBinding(source));
    return;
  }
  if (auto brush = Resource(owner, L"DividerStrokeColorDefaultBrush").try_as<M::Brush>())
    shape.Fill(brush);
}
inline void PaintDivider(C::Border const &border, X::FrameworkElement const &owner) {
  if (DarkTheme(owner)) {
    border.BorderBrush(DarkDivider());
    return;
  }
  if (auto source = QuickDivider()) {
    border.SetBinding(C::Border::BorderBrushProperty(), DividerBinding(source));
    return;
  }
  if (auto brush = Resource(owner, L"DividerStrokeColorDefaultBrush").try_as<M::Brush>())
    border.BorderBrush(brush);
}
inline void PaintDivider(C::StackPanel const &panel, X::FrameworkElement const &owner) {
  if (DarkTheme(owner)) {
    panel.BorderBrush(DarkDivider());
    return;
  }
  if (auto brush = Resource(owner, L"DividerStrokeColorDefaultBrush").try_as<M::Brush>())
    panel.BorderBrush(brush);
}
// The 38 DIP row step in whole pixels for the current scale (100%: 38,
// 125%: 48, 150%: 57). Fixed DIP values tuned for one scale land on different
// pixel steps at another.
inline double StepPx(double scale) { return std::ceil(38 * scale - 1e-6); }
// Restores the binding, not just the value it happened to produce.
struct Journal {
  std::vector<std::function<void()>> undo;
  std::set<std::pair<void *, void *>> saved;
  void Save(X::DependencyObject const &object, X::DependencyProperty const &property) {
    auto key = std::make_pair(winrt::get_abi(object), winrt::get_abi(property));
    if (!saved.insert(key).second)
      return;
    auto value = object.ReadLocalValue(property);
    D::BindingBase binding{nullptr};
    if (auto e = object.try_as<X::FrameworkElement>())
      if (auto expression = e.GetBindingExpression(property))
        binding = expression.ParentBinding();
    undo.push_back([object, property, value, binding] {
      if (binding)
        D::BindingOperations::SetBinding(object, property, binding);
      else if (value == X::DependencyProperty::UnsetValue())
        object.ClearValue(property);
      else
        object.SetValue(property, value);
    });
  }
  void Restore() {
    for (auto it = undo.rbegin(); it != undo.rend(); ++it)
      try {
        (*it)();
      } catch (...) {
        Wh_Log(L"Native restoration: 0x%08X", winrt::to_hresult());
      }
    undo.clear();
    saved.clear();
  }
  ~Journal() { Restore(); }
};
// Members are looked up once per (type, name): the shell's metadata provider was
// asked for the type and member on every read, hundreds of times per opening.
[[clang::no_destroy]] inline std::map<std::wstring, X::Markup::IXamlMember> modelMembers;
inline X::Markup::IXamlMember ModelMember(IInspectable const &model, PCWSTR name) {
  try {
    if (!model)
      return nullptr;
    auto key = Class(model) + L"|" + name;
    if (auto it = modelMembers.find(key); it != modelMembers.end())
      return it->second;
    X::Markup::IXamlMember member{nullptr};
    if (auto provider = X::Application::Current().try_as<X::Markup::IXamlMetadataProvider>())
      if (auto type = provider.GetXamlType(winrt::hstring(Class(model))))
        member = type.GetMember(name);
    modelMembers[key] = member;
    return member;
  } catch (...) {
  }
  return nullptr;
}
inline IInspectable ModelValue(IInspectable const &model, PCWSTR name) {
  if (!model)
    return nullptr;
  try {
    if (auto p = model.try_as<D::ICustomPropertyProvider>())
      if (auto v = p.GetCustomProperty(name); v && v.CanRead())
        return v.GetValue(model);
    if (auto member = ModelMember(model, name))
      return member.GetValue(model);
  } catch (...) {
  }
  return nullptr;
}
inline IInspectable NumberLike(IInspectable const &current, double number) {
  auto value = current.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (!value)
    return nullptr;
  using T = winrt::Windows::Foundation::PropertyType;
  switch (value.Type()) {
  case T::Int32:
    return winrt::box_value(int32_t(std::round(number)));
  case T::UInt32:
    return winrt::box_value(uint32_t(std::round(number)));
  case T::Single:
    return winrt::box_value(float(number));
  case T::Double:
    return winrt::box_value(number);
  default:
    return nullptr;
  }
}
inline bool SetNumber(IInspectable const &model, PCWSTR name, double number) {
  auto next = NumberLike(ModelValue(model, name), number);
  if (!next)
    return false;
  if (auto provider = model.try_as<D::ICustomPropertyProvider>())
    if (auto member = provider.GetCustomProperty(name); member && member.CanWrite()) {
      member.SetValue(model, next);
      return true;
    }
  if (auto member = ModelMember(model, name); member && !member.IsReadOnly()) {
    member.SetValue(model, next);
    return true;
  }
  return false;
}
struct ModelAccess {
  std::function<IInspectable(IInspectable const &, PCWSTR)> read;
  std::function<bool(IInspectable const &, PCWSTR, double)> writeNumber;
};
// Let the shell's binding resolver access its model, including private XAML
// property adapters. The displayed Slider and its Binding are never assigned.
struct NumberRelay : winrt::implements<NumberRelay, D::IValueConverter> {
  double wanted;
  bool read = false, wrote = false;
  IInspectable original{nullptr};
  D::IValueConverter converter{nullptr};
  IInspectable parameter{nullptr};
  winrt::hstring language;
  NumberRelay(double value, D::Binding const &binding)
      : wanted(value), converter(binding.Converter()), parameter(binding.ConverterParameter()),
        language(binding.ConverterLanguage()) {}
  IInspectable Convert(IInspectable const &value, X::Interop::TypeName const &,
                       IInspectable const &, winrt::hstring const &) {
    original = value;
    read = true;
    return winrt::box_value(wanted);
  }
  IInspectable ConvertBack(IInspectable const &, X::Interop::TypeName const &type,
                           IInspectable const &, winrt::hstring const &lang) {
    auto value = converter ? converter.ConvertBack(winrt::box_value(wanted), type, parameter,
                                                   language.empty() ? lang : language)
                           : NumberLike(original, wanted);
    wrote = bool(value);
    return value;
  }
};
inline bool SendBoundNumber(C::Slider const &target, IInspectable const &model, double number) {
  auto original = target.GetBindingExpression(P::RangeBase::ValueProperty());
  if (!original || !model)
    return false;
  auto parent = original.ParentBinding();
  if (parent.Mode() != D::BindingMode::TwoWay)
    return false;
  C::Slider proxy;
  proxy.Minimum(target.Minimum());
  proxy.Maximum(target.Maximum());
  auto relay = winrt::make_self<NumberRelay>(number, parent);
  D::Binding binding;
  binding.Source(model);
  binding.Path(parent.Path());
  binding.Mode(D::BindingMode::TwoWay);
  binding.UpdateSourceTrigger(D::UpdateSourceTrigger::Explicit);
  binding.Converter(*relay);
  proxy.SetBinding(P::RangeBase::ValueProperty(), binding);
  try {
    if (relay->read && (relay->converter || NumberLike(relay->original, number)))
      if (auto expression = proxy.GetBindingExpression(P::RangeBase::ValueProperty()))
        expression.UpdateSource();
  } catch (...) {
    proxy.ClearValue(P::RangeBase::ValueProperty());
    throw;
  }
  proxy.ClearValue(P::RangeBase::ValueProperty());
  return relay->wrote && target.GetBindingExpression(P::RangeBase::ValueProperty()) == original;
}
inline std::wstring String(IInspectable const &value) {
  if (!value)
    return {};
  if (auto p = value.try_as<winrt::Windows::Foundation::IPropertyValue>();
      p && p.Type() == winrt::Windows::Foundation::PropertyType::String)
    return std::wstring(p.GetString());
  return {};
}
} // namespace native

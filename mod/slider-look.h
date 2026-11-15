#pragma once
#include "common.h"
#include <optional>
namespace native {
// Draw the same value over the panel for both wheel and thumb gestures.
class VolumeToolTip {
  C::Grid host_{nullptr};
  C::Canvas overlay_{nullptr};
  C::Border plate_{nullptr};
  C::TextBlock label_{nullptr};
  X::DispatcherTimer timer_{nullptr};
  X::DispatcherTimer::Tick_revoker tick_;
  X::FrameworkElement::Unloaded_revoker unloaded_;
  X::FrameworkElement::LayoutUpdated_revoker layout_;
  C::Slider slider_{nullptr};
  std::optional<uint32_t> pointer_;

  void Position() {
    auto thumb = Named(slider_, L"HorizontalThumb");
    auto anchor = thumb ? thumb : slider_.as<X::FrameworkElement>();
    auto point = anchor.TransformToVisual(host_).TransformPoint(
        {float(anchor.ActualWidth() / 2), 0});
    plate_.Measure({INFINITY, INFINITY});
    auto size = plate_.DesiredSize();
    auto left = std::clamp(point.X - size.Width / 2, 0.f,
                           std::max(0.f, float(host_.ActualWidth()) - size.Width));
    auto top = std::max(0.f, point.Y - size.Height - 4);
    auto previousLeft = C::Canvas::GetLeft(plate_);
    auto previousTop = C::Canvas::GetTop(plate_);
    if (!std::isfinite(previousLeft) || std::abs(previousLeft - left) > .25)
      C::Canvas::SetLeft(plate_, left);
    if (!std::isfinite(previousTop) || std::abs(previousTop - top) > .25)
      C::Canvas::SetTop(plate_, top);
  }

public:
  void Hide() noexcept {
    pointer_.reset();
    if (timer_)
      try {
        timer_.Stop();
      } catch (...) {
        Wh_Log(L"VolumeTooltipTimerStopError %08X", winrt::to_hresult());
      }
    unloaded_.revoke();
    layout_.revoke();
    slider_ = nullptr;
    if (host_)
      try {
        uint32_t index;
        if (host_.Children().IndexOf(overlay_, index))
          host_.Children().RemoveAt(index);
      } catch (...) {
        Wh_Log(L"VolumeTooltipCloseError %08X", winrt::to_hresult());
      }
    host_ = nullptr;
  }
  void Show(X::FrameworkElement const &page, C::Slider const &slider, double value) {
    if (!slider.XamlRoot())
      return;
    if (!overlay_) {
      overlay_ = C::Canvas();
      overlay_.IsHitTestVisible(false);
      C::Canvas::SetZIndex(overlay_, 100);
      plate_ = C::Border();
      plate_.IsHitTestVisible(false);
      plate_.CornerRadius({4, 4, 4, 4});
      plate_.Padding({8, 4, 8, 4});
      plate_.BorderThickness({1, 1, 1, 1});
      label_ = C::TextBlock();
      label_.FontSize(12);
      plate_.Child(label_);
      overlay_.Children().Append(plate_);
    }
    if (slider_ != slider) {
      Hide();
      C::Grid host{nullptr};
      for (auto ancestor = Parent(slider); ancestor; ancestor = Parent(ancestor)) {
        if (auto grid = ancestor.try_as<C::Grid>())
          host = grid;
        if (ancestor == page)
          break;
      }
      if (!host)
        return;
      host_ = host;
      C::Grid::SetRowSpan(overlay_, std::max(1u, host_.RowDefinitions().Size()));
      C::Grid::SetColumnSpan(overlay_, std::max(1u, host_.ColumnDefinitions().Size()));
      host_.Children().Append(overlay_);
      slider_ = slider;
      unloaded_ = slider.Unloaded(winrt::auto_revoke, [this](auto &&, auto &&) { Hide(); });
      layout_ = slider.LayoutUpdated(winrt::auto_revoke, [this](auto &&, auto &&) {
        try {
          if (slider_ && host_)
            Position();
        } catch (...) {
          Hide();
        }
      });
      const auto dark = DarkTheme(slider);
      const auto themedBrush = [&](PCWSTR key, uint8_t shade) {
        auto brush = Resource(slider, key).try_as<M::Brush>();
        if (!brush)
          brush = M::SolidColorBrush(winrt::Windows::UI::Color{255, shade, shade, shade});
        return brush;
      };
      plate_.Background(themedBrush(L"ToolTipBackground", dark ? 44 : 250));
      plate_.BorderBrush(themedBrush(L"ToolTipBorderBrush", dark ? 85 : 220));
      label_.Foreground(themedBrush(L"ToolTipForeground", dark ? 255 : 26));
    }
    auto content = std::to_wstring(std::lround(value));
    if (auto converter = slider.ThumbToolTipValueConverter())
      try {
        if (auto converted = converter.Convert(winrt::box_value(value),
                                               winrt::xaml_typename<winrt::hstring>(), nullptr,
                                               L""))
          if (auto convertedText = String(converted); !convertedText.empty())
            content = std::move(convertedText);
      } catch (...) {
      }
    label_.Text(content);
    Position();
    if (!timer_) {
      timer_ = X::DispatcherTimer();
      timer_.Interval(std::chrono::milliseconds(900));
      tick_ = timer_.Tick(winrt::auto_revoke, [this](auto &&, auto &&) { Hide(); });
    }
    timer_.Stop();
    if (!pointer_)
      timer_.Start();
  }
  void Watch(X::FrameworkElement const &page, C::Slider const &slider, Journal &journal,
             std::vector<std::function<void()>> &revoke) {
    auto listen = [&](X::RoutedEvent const &event, X::Input::PointerEventHandler const &callback) {
      auto handler = winrt::box_value(callback);
      slider.AddHandler(event, handler, true);
      revoke.push_back([slider, event, handler] { slider.RemoveHandler(event, handler); });
    };
    auto weakPage = winrt::make_weak(page);
    listen(X::UIElement::PointerPressedEvent(),
           [this, weakPage](IInspectable const &sender, X::Input::PointerRoutedEventArgs const &args) {
             try {
               auto slider = sender.as<C::Slider>();
               auto point = args.GetCurrentPoint(slider);
               if (!slider.IsEnabled() || !point.Properties().IsLeftButtonPressed())
                 return;
               if (auto page = weakPage.get()) {
                 Show(page, slider, slider.Value());
                 if (slider_ == slider) {
                   pointer_ = point.PointerId();
                   timer_.Stop();
                 }
               }
             } catch (...) {
               Hide();
             }
           });
    X::Input::PointerEventHandler finish =
        [this](IInspectable const &sender, X::Input::PointerRoutedEventArgs const &args) {
          try {
            if (pointer_ && sender == slider_ && *pointer_ == args.Pointer().PointerId()) {
              pointer_.reset();
              timer_.Start();
            }
          } catch (...) {
            Hide();
          }
        };
    listen(X::UIElement::PointerReleasedEvent(), finish);
    listen(X::UIElement::PointerCanceledEvent(), finish);
    listen(X::UIElement::PointerCaptureLostEvent(), finish);
    auto changed = slider.ValueChanged(
        winrt::auto_revoke, [this, weakPage](IInspectable const &sender, auto const &args) {
          try {
            if (pointer_ && sender == slider_)
              if (auto page = weakPage.get())
                Show(page, slider_, args.NewValue());
          } catch (...) {
            Hide();
          }
        });
    auto revoker = std::make_shared<decltype(changed)>(std::move(changed));
    revoke.push_back([revoker] { revoker->revoke(); });
    journal.Save(slider, C::Slider::IsThumbToolTipEnabledProperty());
    slider.IsThumbToolTipEnabled(false);
  }
};

inline void ListenVolumeWheel(C::Slider const &slider, X::Input::PointerEventHandler const &callback,
                              std::vector<std::function<void()>> &revoke) {
  auto event = X::UIElement::PointerWheelChangedEvent();
  auto handler = winrt::box_value(callback);
  slider.AddHandler(event, handler, true);
  revoke.push_back([slider, event, handler] { slider.RemoveHandler(event, handler); });
}

// Thin slider look shared by the Sound page and Quick Settings:
// 1 DIP rail, 4 DIP value bar with rounded ends, thumb invisible until hover,
// drag or keyboard focus. The rail and the bar run from the thumb centre at 0
// to the thumb centre at 100, so the bar always ends under the (hidden) thumb
// and 100 fills the whole line; the caller widens the Slider by half a thumb
// on each side through the margin. Native template parts, states, Fill brushes
// and bindings are kept; everything is journaled.
struct SliderLook {
  Journal &journal;
  std::vector<std::function<void()>> &revoke;
  std::set<void *> &seen;
  bool onBase =
      true; // rail on the base acrylic (QS dark block, SV), not on the Sound content layer
  void Set(X::DependencyObject const &object, X::DependencyProperty const &property,
           IInspectable const &value) {
    journal.Save(object, property);
    object.SetValue(property, value);
  }
  template <class R> void Keep(R &&r) {
    auto revoker = std::make_shared<std::decay_t<R>>(std::forward<R>(r));
    revoke.push_back([revoker] { revoker->revoke(); });
  }
  // 32 DIP control with the rail centred: the native template is 44 DIP tall
  // with a 3 DIP top inset that otherwise offsets the rail from the icon.
  void Geometry(C::Slider const &slider) {
    Set(slider, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
    Set(slider, X::FrameworkElement::HeightProperty(), winrt::box_value(32.));
    Set(slider, C::Control::PaddingProperty(), winrt::box_value(X::Thickness{}));
    Set(slider, X::FrameworkElement::VerticalAlignmentProperty(),
        winrt::box_value(X::VerticalAlignment::Center));
    slider.ApplyTemplate();
    if (auto track = Named(slider, L"HorizontalTemplate").try_as<C::Grid>()) {
      Set(track, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
      Set(track, X::FrameworkElement::HeightProperty(), winrt::box_value(32.));
      Set(track, C::Grid::PaddingProperty(), winrt::box_value(X::Thickness{}));
      if (track.RowDefinitions().Size() == 3)
        for (uint32_t i : {0u, 2u})
          Set(track.RowDefinitions().GetAt(i), C::RowDefinition::HeightProperty(),
              winrt::box_value(X::GridLengthHelper::FromValueAndType(1, X::GridUnitType::Star)));
    }
  }
  // The rail in the colour of the block dividers. The native rail stays
  // (transparent): its PointerOver/Pressed/Disabled states animate its Fill,
  // which would override a local Fill. A 1 DIP rectangle in the same template
  // cell takes its place, below the value bar.
  void Rail(X::Shapes::Rectangle const &rail, double half) {
    auto grid = Parent(rail).try_as<C::Panel>();
    if (!grid)
      return;
    X::Shapes::Rectangle mine{nullptr};
    for (auto const &child : grid.Children())
      if (auto r = child.try_as<X::Shapes::Rectangle>(); r && r.Name() == L"NativeSoundRail") {
        mine = r;
        break;
      }
    if (!mine) {
      mine = X::Shapes::Rectangle();
      mine.Name(L"NativeSoundRail");
      mine.IsHitTestVisible(false);
      mine.Height(1);
      mine.VerticalAlignment(X::VerticalAlignment::Center);
      mine.RadiusX(.5);
      mine.RadiusY(.5);
      C::Grid::SetRow(mine, C::Grid::GetRow(rail));
      C::Grid::SetRowSpan(mine, C::Grid::GetRowSpan(rail));
      C::Grid::SetColumn(mine, C::Grid::GetColumn(rail));
      C::Grid::SetColumnSpan(mine, C::Grid::GetColumnSpan(rail));
      uint32_t index = 0;
      if (grid.Children().IndexOf(rail, index))
        grid.Children().InsertAt(index + 1, mine);
      else
        grid.Children().Append(mine);
      auto weakGrid = winrt::make_weak(grid);
      journal.undo.push_back([weakGrid, mine] {
        if (auto g = weakGrid.get()) {
          uint32_t i = 0;
          if (g.Children().IndexOf(mine, i))
            g.Children().RemoveAt(i);
        }
      });
      Set(rail, X::UIElement::OpacityProperty(), winrt::box_value(0.));
      auto weakRail = winrt::make_weak(rail);
      Keep(mine.ActualThemeChanged(
          winrt::auto_revoke,
          [weakRail, onBase = onBase](X::FrameworkElement const &sender, auto &&) {
            if (auto r = weakRail.get())
              if (auto shape = sender.try_as<X::Shapes::Shape>())
                PaintDivider(shape, r, onBase);
          }));
    }
    mine.Margin({half, 0, half, 0});
    PaintDivider(mine, rail, onBase);
  }
  // margin(half) gives the Slider margin for half the thumb width.
  bool Thin(C::Slider const &slider, std::function<X::Thickness(double)> const &margin) {
    auto rail = Named(slider, L"HorizontalTrackRect").try_as<X::Shapes::Rectangle>();
    auto bar = Named(slider, L"HorizontalDecreaseRect").try_as<X::Shapes::Rectangle>();
    auto thumb = Named(slider, L"HorizontalThumb").try_as<P::Thumb>();
    if (!rail || !bar || !thumb)
      return false;
    double half =
        std::max(0., (std::isfinite(thumb.Width()) && thumb.Width() > 0 ? thumb.Width()
                                                                        : thumb.ActualWidth()) /
                         2);
    Set(rail, X::FrameworkElement::HeightProperty(), winrt::box_value(1.));
    Set(rail, X::FrameworkElement::VerticalAlignmentProperty(),
        winrt::box_value(X::VerticalAlignment::Center));
    Set(rail, X::FrameworkElement::MarginProperty(),
        winrt::box_value(X::Thickness{half, 0, half, 0}));
    Set(rail, X::Shapes::Rectangle::RadiusXProperty(), winrt::box_value(.5));
    Set(rail, X::Shapes::Rectangle::RadiusYProperty(), winrt::box_value(.5));
    Set(bar, X::FrameworkElement::HeightProperty(), winrt::box_value(4.));
    Set(bar, X::FrameworkElement::VerticalAlignmentProperty(),
        winrt::box_value(X::VerticalAlignment::Center));
    Set(bar, X::FrameworkElement::MarginProperty(),
        winrt::box_value(X::Thickness{half, 0, -half, 0}));
    Set(bar, X::Shapes::Rectangle::RadiusXProperty(), winrt::box_value(2.));
    Set(bar, X::Shapes::Rectangle::RadiusYProperty(), winrt::box_value(2.));
    Rail(rail, half);
    Set(slider, X::FrameworkElement::MarginProperty(), winrt::box_value(margin(half)));
    if (!seen.insert(winrt::get_abi(slider)).second)
      return true;
    Set(thumb, X::UIElement::OpacityProperty(), winrt::box_value(0.));
    auto previous = thumb.OpacityTransition();
    X::ScalarTransition fade;
    fade.Duration(std::chrono::milliseconds(83));
    thumb.OpacityTransition(fade);
    journal.undo.push_back([thumb, previous] { thumb.OpacityTransition(previous); });
    struct Visible {
      bool over = false, drag = false, focus = false;
    };
    auto state = std::make_shared<Visible>();
    auto weakThumb = winrt::make_weak(thumb);
    auto apply = [state, weakThumb] {
      if (auto t = weakThumb.get())
        t.Opacity(state->over || state->drag || state->focus ? 1. : 0.);
    };
    auto entered = winrt::box_value(X::Input::PointerEventHandler([state, apply](auto &&, auto &&) {
      state->over = true;
      apply();
    }));
    auto exited = winrt::box_value(X::Input::PointerEventHandler([state, apply](auto &&, auto &&) {
      state->over = false;
      apply();
    }));
    slider.AddHandler(X::UIElement::PointerEnteredEvent(), entered, true);
    slider.AddHandler(X::UIElement::PointerExitedEvent(), exited, true);
    revoke.push_back([slider, entered, exited] {
      slider.RemoveHandler(X::UIElement::PointerEnteredEvent(), entered);
      slider.RemoveHandler(X::UIElement::PointerExitedEvent(), exited);
    });
    Keep(thumb.DragStarted(winrt::auto_revoke, [state, apply](auto &&, auto &&) {
      state->drag = true;
      apply();
    }));
    Keep(thumb.DragCompleted(winrt::auto_revoke, [state, apply](auto &&, auto &&) {
      state->drag = false;
      apply();
    }));
    // Pressing the track moves the value without a thumb drag: show it while pressed.
    auto pressed = winrt::box_value(X::Input::PointerEventHandler([state, apply](auto &&, auto &&) {
      state->drag = true;
      apply();
    }));
    auto released =
        winrt::box_value(X::Input::PointerEventHandler([state, apply](auto &&, auto &&) {
          state->drag = false;
          apply();
        }));
    slider.AddHandler(X::UIElement::PointerPressedEvent(), pressed, true);
    for (auto ev : {X::UIElement::PointerReleasedEvent(), X::UIElement::PointerCaptureLostEvent(),
                    X::UIElement::PointerCanceledEvent()})
      slider.AddHandler(ev, released, true);
    revoke.push_back([slider, pressed, released] {
      slider.RemoveHandler(X::UIElement::PointerPressedEvent(), pressed);
      for (auto ev : {X::UIElement::PointerReleasedEvent(), X::UIElement::PointerCaptureLostEvent(),
                      X::UIElement::PointerCanceledEvent()})
        slider.RemoveHandler(ev, released);
    });
    Keep(slider.GotFocus(winrt::auto_revoke,
                         [state, apply, weak = winrt::make_weak(slider)](auto &&, auto &&) {
                           if (auto s = weak.get())
                             state->focus = s.FocusState() == X::FocusState::Keyboard;
                           apply();
                         }));
    Keep(slider.LostFocus(winrt::auto_revoke, [state, apply](auto &&, auto &&) {
      state->focus = false;
      apply();
    }));
    return true;
  }
};
} // namespace native

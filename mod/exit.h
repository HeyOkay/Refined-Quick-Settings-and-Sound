#pragma once
#include "common.h"
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.System.h>
namespace native {
// Hiding the page being left (Quick Settings on the way to Sound, Sound on the
// way back): opacity 1 -> 0 in 200 ms, decelerating; the shell's entrance of
// the new page stays as it is.
// The shell shows the leaving page as its own copy (IDuplicateVisual), which
// the mod cannot animate or move. So the button's command is wrapped: on
// Execute the page is rendered once (RenderTargetBitmap), the picture is laid
// over the panel, the page is made transparent (so the shell's copy shows
// nothing), the original command runs, and the picture fades in composition.
// The picture is bottom-aligned in RootGrid: the panel grows and shrinks from
// its bottom edge, so it stays where it was on screen while the panel's height
// changes. It hangs from a zero-height Canvas, so it never takes part in
// layout, and is clipped to RootGrid.
// The shell takes its copy from what XAML last rendered, so the page is made
// transparent in XAML and the command runs only after a frame with that state
// (and the picture) has been rendered. For Sound the copied element is L2Frame,
// the parent of the page; for Quick Settings it is L1Grid itself and the
// shell's fade starts the copy at full opacity, so for a panel its children
// are made transparent instead (Quick Settings restores them when shown).
inline void HideNow(X::UIElement const &e) {
  if (!e)
    return;
  if (auto panel = e.try_as<C::Panel>()) {
    for (auto const &child : panel.Children())
      child.Opacity(0);
    return;
  }
  e.Opacity(0);
}
inline void AfterRender(std::function<void()> next) {
  struct State {
    winrt::event_token token{};
    int frames = 0;
    bool done = false;
  };
  auto state = std::make_shared<State>();
  auto finish = [state, next] {
    if (state->done)
      return;
    state->done = true;
    try {
      M::CompositionTarget::Rendering(state->token);
    } catch (...) {
    }
    next();
  };
  // Run `next` after the Rendering callback, not inside it: navigating from
  // inside Rendering renders the native Sound page once before the mod has
  // restructured it.
  auto post = [finish] {
    try {
      if (auto q = winrt::Windows::System::DispatcherQueue::GetForCurrentThread())
        if (q.TryEnqueue([finish] { finish(); }))
          return;
    } catch (...) {
    }
    finish();
  };
  try {
    state->token = M::CompositionTarget::Rendering([state, post](auto &&, auto &&) {
      if (++state->frames == 2)
        post();
    });
  } catch (...) {
    finish();
  }
}
// Fades an element's visual to 0 (composition, independent of the UI thread);
// ShowAgain gives it its opacity back.
inline void FadeOutNow(X::UIElement const &e, int ms) {
  try {
    auto v = X::Hosting::ElementCompositionPreview::GetElementVisual(e);
    auto c = v.Compositor();
    auto a = c.CreateScalarKeyFrameAnimation();
    using winrt::Windows::Foundation::Numerics::float2;
    a.InsertKeyFrame(0, 1);
    a.InsertKeyFrame(1, 0, c.CreateCubicBezierEasingFunction(float2{0, 0}, float2{0, 1}));
    a.Duration(std::chrono::milliseconds(ms));
    v.StartAnimation(L"Opacity", a);
  } catch (...) {
  }
}
inline void AfterDelay(int ms, std::function<void()> next) {
  try {
    auto q = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    if (!q) {
      next();
      return;
    }
    auto timer = q.CreateTimer();
    timer.Interval(std::chrono::milliseconds(ms));
    timer.IsRepeating(false);
    auto keep = std::make_shared<winrt::Windows::System::DispatcherQueueTimer>(timer);
    timer.Tick([keep, next](auto &&, auto &&) {
      try {
        keep->Stop();
      } catch (...) {
      }
      next();
    });
    timer.Start();
  } catch (...) {
    next();
  }
}
inline void ShowAgain(X::UIElement const &e) {
  if (!e)
    return;
  try {
    auto v = X::Hosting::ElementCompositionPreview::GetElementVisual(e);
    v.StopAnimation(L"Opacity");
    v.Opacity(1);
  } catch (...) {
  }
}
struct DelayedCommand : winrt::implements<DelayedCommand, X::Input::ICommand> {
  X::Input::ICommand original;
  std::function<void(IInspectable const &)> run;
  DelayedCommand(X::Input::ICommand o, std::function<void(IInspectable const &)> r)
      : original(std::move(o)), run(std::move(r)) {}
  bool CanExecute(IInspectable const &p) { return original.CanExecute(p); }
  void Execute(IInspectable const &p) { run(p); }
  winrt::event_token
  CanExecuteChanged(winrt::Windows::Foundation::EventHandler<IInspectable> const &h) {
    return original.CanExecuteChanged(h);
  }
  void CanExecuteChanged(winrt::event_token const &t) noexcept {
    try {
      original.CanExecuteChanged(t);
    } catch (...) {
    }
  }
};
constexpr int exitDuration = 200;
inline winrt::Windows::UI::Composition::CompositionEasingFunction
ExitEasing(winrt::Windows::UI::Composition::Compositor const &c) {
  using winrt::Windows::Foundation::Numerics::float2;
  return c.CreateCubicBezierEasingFunction(float2{0, 0}, float2{0, 1});
}
// snapshot: element rendered and laid over (L1Grid, L2Frame); hide: element
// made transparent before the shell takes its copy (L1Grid, the Sound page).
inline void ExitWithSnapshot(X::FrameworkElement const &snapshot, X::FrameworkElement const &hide,
                             std::function<void()> proceed) {
  auto host = Parent(snapshot).try_as<C::Grid>();
  if (!host || snapshot.ActualHeight() < 1 || snapshot.ActualWidth() < 1) {
    proceed();
    return;
  }
  M::Imaging::RenderTargetBitmap natural;
  auto weakSnapshot = winrt::make_weak(snapshot), weakHide = winrt::make_weak(hide);
  auto weakHost = winrt::make_weak(host);
  // Lays the picture over the panel; pw x ph is the natural render size in pixels.
  std::function<void(M::Imaging::RenderTargetBitmap, int, int,
                     winrt::Windows::Foundation::AsyncStatus)>
      show = [=](M::Imaging::RenderTargetBitmap bitmap, int pw, int ph,
                 winrt::Windows::Foundation::AsyncStatus status) {
        auto s = weakSnapshot.get();
        auto h = weakHost.get();
        if (status != winrt::Windows::Foundation::AsyncStatus::Completed || !s || !h) {
          proceed();
          return;
        }
        C::Grid box{nullptr};
        try {
          auto at = s.TransformToVisual(h).TransformPoint({0, 0});
          double w = s.ActualWidth(), ht = s.ActualHeight();
          double bottom = h.ActualHeight() - (at.Y + ht);
          box = C::Grid();
          box.IsHitTestVisible(false);
          C::Canvas::SetZIndex(box, 1000);
          C::Grid::SetRowSpan(box, std::max(1u, h.RowDefinitions().Size()));
          C::Grid::SetColumnSpan(box, std::max(1u, h.ColumnDefinitions().Size()));
          // The bitmap can cover more than the element's layout box (items with
          // negative margins): natural size, top-left at the element's origin.
          auto root = s.XamlRoot();
          double scale = root ? root.RasterizationScale() : 1.;
          double iw = pw > 0 ? pw / scale : w, ih = ph > 0 ? ph / scale : ht;
          C::Image image;
          image.Source(bitmap);
          image.Stretch(M::Stretch::Fill);
          image.Width(iw);
          image.Height(ih);
          C::Canvas anchor;
          anchor.Height(0);
          anchor.VerticalAlignment(X::VerticalAlignment::Bottom);
          anchor.Margin({0, 0, 0, bottom});
          C::Canvas::SetLeft(image, at.X);
          C::Canvas::SetTop(image, -ht);
          anchor.Children().Append(image);
          box.Children().Append(anchor);
          // Clipped at the host's edges (the panel shrinking under Sound's picture).
          {
            M::RectangleGeometry geometry;
            geometry.Rect({0, 0, float(h.ActualWidth()), float(h.ActualHeight())});
            box.Clip(geometry);
            auto weakClip = winrt::make_weak(geometry);
            box.SizeChanged([weakClip](auto &&, X::SizeChangedEventArgs const &args) {
              if (auto g = weakClip.get())
                g.Rect({0, 0, args.NewSize().Width, args.NewSize().Height});
            });
          }
          h.Children().Append(box);
          if (auto e = weakHide.get())
            HideNow(e);
          AfterRender(proceed);
          namespace Comp = winrt::Windows::UI::Composition;
          auto v = X::Hosting::ElementCompositionPreview::GetElementVisual(image);
          auto c = v.Compositor();
          auto ease = ExitEasing(c);
          auto duration = std::chrono::milliseconds(exitDuration);
          auto batch = c.CreateScopedBatch(Comp::CompositionBatchTypes::Animation);
          auto fade = c.CreateScalarKeyFrameAnimation();
          fade.InsertKeyFrame(0, 1);
          fade.InsertKeyFrame(1, 0, ease);
          fade.Duration(duration);
          v.StartAnimation(L"Opacity", fade);
          batch.End();
          auto weakBox = winrt::make_weak(box);
          batch.Completed([weakHost, weakBox](auto &&, auto &&) {
            if (auto h2 = weakHost.get())
              if (auto b = weakBox.get()) {
                uint32_t i = 0;
                if (h2.Children().IndexOf(b, i))
                  h2.Children().RemoveAt(i);
              }
          });
        } catch (...) {
          Wh_Log(L"ExitSnapshotError %08X", winrt::to_hresult());
          if (box)
            try {
              uint32_t i = 0;
              if (h.Children().IndexOf(box, i))
                h.Children().RemoveAt(i);
            } catch (...) {
            }
          if (auto e = weakHide.get())
            HideNow(e);
          proceed();
        }
      };
  // Animated (Lottie) glyphs come out pixelated at the natural size while text
  // is sharp: a second render at twice the size, shown at the natural size.
  // Falls back to the natural render.
  try {
    natural.RenderAsync(snapshot).Completed(
        [=](auto const &, winrt::Windows::Foundation::AsyncStatus status) {
          int pw = natural.PixelWidth(), ph = natural.PixelHeight();
          auto s = weakSnapshot.get();
          if (status != winrt::Windows::Foundation::AsyncStatus::Completed || !s || pw <= 0 ||
              ph <= 0) {
            show(natural, pw, ph, status);
            return;
          }
          try {
            M::Imaging::RenderTargetBitmap twice;
            twice.RenderAsync(s, pw * 2, ph * 2)
                .Completed([=](auto const &, winrt::Windows::Foundation::AsyncStatus second) {
                  if (second == winrt::Windows::Foundation::AsyncStatus::Completed &&
                      twice.PixelWidth() > pw)
                    show(twice, pw, ph, second);
                  else
                    show(natural, pw, ph, status);
                });
          } catch (...) {
            show(natural, pw, ph, status);
          }
        });
  } catch (...) {
    Wh_Log(L"ExitSnapshotError %08X", winrt::to_hresult());
    proceed();
  }
}
// The wrapper's Execute; with animations off the command runs at once.
inline void Leave(X::FrameworkElement const &snapshot, X::FrameworkElement const &hide,
                  X::Input::ICommand const &original, IInspectable const &parameter) {
  auto proceed = [original, parameter] {
    try {
      original.Execute(parameter);
    } catch (...) {
    }
  };
  if (winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
    ExitWithSnapshot(snapshot, hide, proceed);
    return;
  }
  proceed();
}
} // namespace native

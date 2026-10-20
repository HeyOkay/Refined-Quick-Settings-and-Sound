#pragma once
#include "common.h"
#include "volume-icon.h"
#include "slider-look.h"
#include "exit.h"
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
namespace native {
struct Menu {
  C::Button button{nullptr};
  C::TextBlock label{nullptr};
  C::FontIcon endpointIcon{nullptr};
  C::Flyout flyout{nullptr};
  C::ListView list{nullptr};
  X::FrameworkElement arrow{nullptr};
  X::Media::Animation::Storyboard arrowStory{nullptr};
  bool open = false;
};
struct AppRow {
  winrt::weak_ref<C::Grid> root;
  winrt::weak_ref<C::Slider> slider;
  winrt::weak_ref<P::ToggleButton> toggle;
  C::Border badge{nullptr};
  C::TextBlock level{nullptr};
  C::Button levelButton{nullptr}, levelMute{nullptr};
  std::shared_ptr<AnimatedVolume> levelIcon;
  double last = 50, before = 0, expected = NAN;
  ULONGLONG verifyAt = 0;
  IInspectable model{nullptr};
  bool beforeValid = false;
  ULONGLONG unnamedSince = 0;
  std::wstring name;
  IInspectable nameModel{nullptr};
  double wheelTarget = NAN;
  ULONGLONG wheelAt = 0;
};
// Last complete Core Audio snapshot from any Sound page. A new page's worker
// has not read anything during the first frames (blank Input, master at 0).
inline sound::Snapshot lastAudio;
inline bool soundOpenedOnce = false;
class SoundPage {
  X::FrameworkElement page_{nullptr}, outputAnchor_{nullptr}, frame_{nullptr};
  Journal journal_;
  // L2Frame outlives the page (it hosts every Sound page): its changes are kept
  // apart so an abandoned page still gives the frame its native height back.
  Journal frameJournal_;
  HWND host_ = nullptr;
  std::shared_ptr<double> ownedHeight_ = std::make_shared<double>(NAN);
  double wantedHeight_ = 0;
  ModelAccess access_;
  std::vector<std::shared_ptr<Menu>> menus_;
  std::vector<std::shared_ptr<AppRow>> rows_;
  std::vector<std::function<void()>> revoke_;
  C::ListView mixerList_{nullptr};
  C::Button mixerButton_{nullptr};
  X::FrameworkElement arrow_{nullptr};
  C::StackPanel inputBlock_{nullptr};
  std::shared_ptr<Menu> input_;
  C::Slider master_{nullptr};
  C::Button masterIcon_{nullptr};
  C::Grid footer_{nullptr};
  C::Border footerBorder_{nullptr};
  X::Style pickerBase_{nullptr};
  std::shared_ptr<AnimatedVolume> masterAnimation_;
  std::function<void()> volumeChanged_;
  bool preparing_ = false, prepared_ = false, rowsDirty_ = true;
  std::vector<std::pair<winrt::weak_ref<C::TextBlock>, M::TranslateTransform>> textBaselines_;
  sound::Audio audio_;
  bool sync_ = false, expanded_ = true, restoring_ = false;
  int glyph_ = -1;
  std::wstring inputSignature_;
  sound::Snapshot latest_;
  unsigned long long pending_ = 0;
  std::set<void *> appIds_;
  std::wstring pendingInputId_;
  unsigned long long pendingInput_ = 0;
  X::DispatcherTimer gate_{nullptr};
  winrt::event_token gateTick_{};
  ULONGLONG gateStarted_ = 0;
  int gateStable_ = 0; // Snapshot of the VM rows, shared with RenderAsync's completion handler.
  struct MixerSnapshot {
    std::mutex lock;
    M::Imaging::RenderTargetBitmap bitmap{nullptr};
    std::vector<void *> items;
    double height = 0;
    ULONGLONG at = 0;
    bool capturing = false;
  };
  std::shared_ptr<MixerSnapshot> snapshot_ = std::make_shared<MixerSnapshot>();
  M::ImageBrush snapshotBrush_{nullptr};
  X::UIElement itemsHost_{nullptr};
  IInspectable savedBackground_{nullptr};
  bool crossfade_ = false, holdAnimation_ = false;
  double stableListHeight_ = -1;
  bool hideUnnamed_ = false, holding_ = false, pendingRows_ = false;
  ULONGLONG holdStarted_ = 0;
  int holdStable_ = 0;
  std::wstring holdSignature_;
  std::vector<void *> rowItems_;
  X::DispatcherTimer holdTimer_{nullptr};
  winrt::event_token holdTick_{};
  X::DispatcherTimer rowsTimer_{nullptr};
  winrt::event_token rowsTick_{};
  ULONGLONG layoutWorkTick_ = 0;
  int layoutWorkCount_ = 0;
  std::wstring rowSignature_;
  bool fading_ = false;
  std::wstring gateLabels_;
  bool gating_ = false;
  double heldHeight_ = 0;
  int savedExpanded_ = -1, savedCollapsed_ = -1;
  Journal gateJournal_;
  X::DispatcherTimer animation_{nullptr};
  winrt::event_token animationTick_{};
  double start_ = 0;
  double from_ = 0, to_ = 0, arrowStart_ = 0;
  X::Media::Animation::Storyboard gearStory_{nullptr};
  C::TextBlock pageTitle_{nullptr};
  // VM rows on opening: the new page first shows the previous session list, then
  // the shell replaces it; the rows are hidden until the list settles, then fade.
  bool openHold_ = false, openRebuilt_ = false;
  // Page title ("Sound"): its font family for the app volume numbers.
  // The native QA_All_Settings animation in FooterIconButtonStyle, as the Quick
  // Settings gear: it turns while the button is held.
  void SettingsGear(C::Button const &button) {
    try {
      if (auto style = Resource(page_, L"FooterIconButtonStyle").try_as<X::Style>()) {
        button.Style(style);
        button.Width(36);
        button.Height(36);
        button.Margin({-2, -2, -2, -2});
      }
      auto factory = NativeAnimatedFactory();
      IInspectable inner{nullptr};
      auto icon = factory.as<U::IAnimatedIconFactory>().CreateInstance(nullptr, inner);
      auto statics = factory.as<U::IAnimatedIconStatics>();
      icon.Source(NativeSource(L"ControlCenter.QA_All_Settings"));
      icon.Width(16);
      icon.Height(16);
      icon.IsHitTestVisible(false);
      button.Content(icon);
      // The footer style writes AnimatedIcon.State on the button; the icon follows it.
      auto token = button.RegisterPropertyChangedCallback(
          statics.StateProperty(),
          [statics, icon](X::DependencyObject const &sender, X::DependencyProperty const &) {
            try {
              statics.SetState(icon, statics.GetState(sender));
            } catch (...) {
            }
          });
      revoke_.push_back([button, statics, token] {
        button.UnregisterPropertyChangedCallback(statics.StateProperty(), token);
      });
    } catch (...) {
      Wh_Log(L"SoundGearIconError %08X", winrt::to_hresult());
      button.Content(SymbolGlyph(page_, 0xe713, 14));
    }
  }
  template <class R> void Keep(R &&r) {
    auto revoker = std::make_shared<std::decay_t<R>>(std::forward<R>(r));
    revoke_.push_back([revoker] { revoker->revoke(); });
  }
  static void Rotate(X::FrameworkElement const &glyph, X::Media::Animation::Storyboard &previous,
                     double target, int milliseconds) {
    namespace A = X::Media::Animation;
    auto rotation = glyph.RenderTransform().try_as<M::RotateTransform>();
    if (!rotation) {
      rotation = M::RotateTransform();
      glyph.RenderTransformOrigin({.5f, .5f});
      glyph.RenderTransform(rotation);
    }
    auto from = rotation.Angle();
    if (previous)
      previous.Stop();
    rotation.Angle(from);
    if (!winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
      rotation.Angle(target);
      return;
    }
    A::DoubleAnimationUsingKeyFrames turn;
    A::SplineDoubleKeyFrame key;
    key.Value(target);
    key.KeyTime(A::KeyTimeHelper::FromTimeSpan(std::chrono::milliseconds(milliseconds)));
    A::KeySpline spline;
    spline.ControlPoint1({.1f, .9f});
    spline.ControlPoint2({.2f, 1.f});
    key.KeySpline(spline);
    turn.KeyFrames().Append(key);
    A::Storyboard::SetTarget(turn, glyph);
    A::Storyboard::SetTargetProperty(turn, L"(UIElement.RenderTransform).(RotateTransform.Angle)");
    previous = A::Storyboard();
    previous.Children().Append(turn);
    previous.Begin();
  }
  static double PanelEase(double progress) {
    double low = 0, high = 1, t = progress;
    for (int i = 0; i < 20; i++) {
      t = (low + high) / 2;
      double x = 3 * (1 - t) * (1 - t) * t * .1 + 3 * (1 - t) * t * t * .2 + t * t * t;
      if (x < progress)
        low = t;
      else
        high = t;
    }
    return 3 * (1 - t) * (1 - t) * t * .9 + 3 * (1 - t) * t * t + t * t * t;
  }
  void Set(X::DependencyObject const &object, X::DependencyProperty const &property,
           IInspectable const &value) {
    journal_.Save(object, property);
    object.SetValue(property, value);
  }
  void TextLine(C::TextBlock const &text, double height) {
    Set(text, C::TextBlock::LineHeightProperty(), winrt::box_value(height));
    Set(text, C::TextBlock::LineStackingStrategyProperty(),
        winrt::box_value(X::LineStackingStrategy::BlockLineHeight));
    Set(text, C::TextBlock::TextLineBoundsProperty(), winrt::box_value(X::TextLineBounds::Full));
    Set(text, C::TextBlock::PaddingProperty(), winrt::box_value(X::Thickness{}));
    for (auto const &entry : textBaselines_)
      if (entry.first.get() == text)
        return;
    M::TransformGroup transforms;
    if (auto previous = text.RenderTransform())
      transforms.Children().Append(previous);
    M::TranslateTransform optical;
    transforms.Children().Append(optical);
    Set(text, X::UIElement::RenderTransformProperty(), transforms);
    textBaselines_.push_back({winrt::make_weak(text), optical});
  }
  void AlignTextBaselines() {
    auto root = page_.XamlRoot();
    double scale = root ? root.RasterizationScale() : 1.;
    // The canonical browser's zero-height inline probe measures a 15.2 DIP
    // baseline in its 14px body text at 125%. XAML BlockLineHeight scales its
    // baseline instead. Correct that measured optical difference on whole
    // physical pixels, keeping the line box and all hit targets unchanged.
    for (auto const &entry : textBaselines_)
      if (auto text = entry.first.get(); text && text.IsLoaded() && text.ActualHeight() > 0) {
        double target = 15.2 * text.FontSize() / 14.;
        double delta = std::round((target - text.BaselineOffset()) * scale) / scale;
        if (std::abs(entry.second.Y() - delta) > .001)
          entry.second.Y(delta);
      }
  }
  double HeadingBottom() const {
    auto root = page_.XamlRoot();
    double scale = root ? root.RasterizationScale() : 1.;
    return 9 - (std::abs(scale - 1.25) < .001 ? 1 / scale : 0.);
  }
  void NormalizeGroup(X::FrameworkElement start) {
    for (auto p = Parent(start); p && p != page_; p = Parent(p))
      if (auto grid = p.try_as<C::GridView>()) {
        Set(grid, C::Control::PaddingProperty(), winrt::box_value(X::Thickness{}));
        break;
      }
  }
  void SliderGeometry(C::Slider const &slider) {
    SliderLook{journal_, revoke_, thinSliders_}.Geometry(slider);
    ThinSlider(slider);
  }
  // Glyphs centred on the row/button height. At 125% (40 px rows/buttons, 20 px
  // glyph) the native insets put dropdown rows 1 px high and closed OD/ID/SS
  // buttons 1 px low. Margins, not transforms, keep the glyphs on the pixel grid.
  static constexpr double listGlyphDrop = .8, buttonGlyphRaise = .8;
  std::set<void *> thinSliders_;
  // Thin look: SliderLook. VM rows keep the right edge on the badge column.
  void ThinSlider(C::Slider const &slider) {
    SliderLook{journal_, revoke_, thinSliders_, slider == master_}.Thin(
        slider, [this, slider](double half) {
          return slider == master_ ? X::Thickness{12 - half, 0, 12 - half, 0}
                                   : X::Thickness{12 - half, 0, -half, 0};
        });
  }
  void AlignRows() {
    auto root = page_.XamlRoot();
    double scale = root ? root.RasterizationScale() : 1.;
    unsigned visible = 0;
    for (uint32_t i = 0; i < mixerList_.Items().Size(); i++)
      if (auto container = mixerList_.ContainerFromIndex(i).try_as<C::ListViewItem>();
          container && container.Visibility() != X::Visibility::Collapsed) {
        // Quantize cumulative positions, not each 38 DIP row independently
        // (at 125% one step is 47.5 px: 47/48).
        double before = std::floor(visible * 38 * scale + 1e-6),
               after = std::floor((visible + 1) * 38 * scale + 1e-6);
        double height = (after - before) / scale;
        visible++;
        Set(container, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
        Set(container, X::FrameworkElement::HeightProperty(), winrt::box_value(height));
        Find(container, [&](auto const &e) {
          if (auto slider = e.template try_as<C::Slider>();
              slider && slider.Name() == L"AppVolumeLevel")
            if (auto row = Parent(slider).template try_as<C::Grid>())
              Set(row, X::FrameworkElement::HeightProperty(), winrt::box_value(height));
          return false;
        });
      }
  }
  void CompactRealized(std::shared_ptr<Menu> const &menu) {
    auto list = menu->list;
    if (!list)
      return;
    for (uint32_t i = 0; i < list.Items().Size(); i++) {
      auto item = list.ContainerFromIndex(i).try_as<C::ListViewItem>();
      // Explicit input containers can be prepared before Flyout.Opened and the
      // first visible frame; native data-backed menus still use realized rows.
      if (!item && menu == input_)
        item = list.Items().GetAt(i).try_as<C::ListViewItem>();
      if (!item)
        continue;
      Set(item, X::FrameworkElement::HeightProperty(), winrt::box_value(32.));
      Set(item, X::FrameworkElement::MinHeightProperty(), winrt::box_value(32.));
      Set(item, X::FrameworkElement::MarginProperty(),
          winrt::box_value(X::Thickness{0, 0, 0, i + 1 < list.Items().Size() ? 4. : 0.}));
      // A 1 px border is snapped to one physical pixel at this XamlRoot scale.
      // Include that transparent border in the content inset, not in the native
      // presenter's background inset: 5 DIP above, 6 below.
      auto root = list.XamlRoot();
      if (!root)
        root = menu->button.XamlRoot();
      double border = 1. / (root ? root.RasterizationScale() : 1.);
      // 7 instead of 11 with the button 4 DIP narrower on each side (20 DIP from
      // the panel edges, like the SV mute/settings plates): glyph axes kept.
      X::Thickness inset{7 + border, 5 + border, 7 + border, 6 + border};
      Set(item, C::Control::PaddingProperty(), winrt::box_value(inset));
      Set(item, C::Control::HorizontalContentAlignmentProperty(),
          winrt::box_value(X::HorizontalAlignment::Stretch));
      Set(item, C::Control::VerticalContentAlignmentProperty(),
          winrt::box_value(X::VerticalAlignment::Stretch));
      item.ApplyTemplate();
      Find(item, [&](auto const &e) {
        if (auto presenter = e.template try_as<P::ListViewItemPresenter>()) {
          // Height on ListViewItem does not override a minimum on its presenter.
          // Normalize the actual template root as well, retaining its native states.
          Set(presenter, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
          Set(presenter, X::FrameworkElement::HeightProperty(), winrt::box_value(32.));
          Set(presenter, X::FrameworkElement::MarginProperty(), winrt::box_value(X::Thickness{}));
          Set(presenter, C::ContentPresenter::PaddingProperty(), winrt::box_value(X::Thickness{}));
          Set(presenter, C::ContentPresenter::BorderThicknessProperty(),
              winrt::box_value(X::Thickness{}));
          Set(presenter, C::ContentPresenter::HorizontalContentAlignmentProperty(),
              winrt::box_value(X::HorizontalAlignment::Stretch));
          Set(presenter, C::ContentPresenter::VerticalContentAlignmentProperty(),
              winrt::box_value(X::VerticalAlignment::Stretch));
          Set(presenter, P::ListViewItemPresenter::ContentMarginProperty(),
              winrt::box_value(inset));
          Set(presenter, P::ListViewItemPresenter::PointerOverBackgroundMarginProperty(),
              winrt::box_value(X::Thickness{}));
          Set(presenter, P::ListViewItemPresenter::SelectionIndicatorModeProperty(),
              winrt::box_value(P::ListViewItemPresenterSelectionIndicatorMode::Overlay));
          Set(presenter, P::ListViewItemPresenter::SelectionIndicatorVisualEnabledProperty(),
              winrt::box_value(true));
          // This shell template has separate Border margins, which survive the
          // presenter's PointerOverBackgroundMargin. Normalize those actual nodes.
          for (int childIndex = 0; childIndex < M::VisualTreeHelper::GetChildrenCount(presenter);
               childIndex++)
            if (auto border = M::VisualTreeHelper::GetChild(presenter, childIndex)
                                  .template try_as<C::Border>())
              Set(border, X::FrameworkElement::MarginProperty(), winrt::box_value(X::Thickness{}));
        }
        if (auto text = e.template try_as<C::TextBlock>()) {
          if (text.Name() == L"TitleText") {
            // Constrain the native DataTemplate root, rather than translating its
            // descendants. Keep all children, including the broadcast-audio command.
            if (auto content = Parent(text).template try_as<C::Grid>()) {
              Set(content, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
              Set(content, X::FrameworkElement::MarginProperty(), winrt::box_value(X::Thickness{}));
              Set(content, C::Grid::PaddingProperty(), winrt::box_value(X::Thickness{}));
              Set(content, X::FrameworkElement::VerticalAlignmentProperty(),
                  winrt::box_value(X::VerticalAlignment::Stretch));
            }
            TextLine(text, 19.);
            Set(text, C::TextBlock::TextTrimmingProperty(),
                winrt::box_value(X::TextTrimming::CharacterEllipsis));
            Set(text, C::TextBlock::TextWrappingProperty(),
                winrt::box_value(X::TextWrapping::NoWrap));
            Set(text, C::ToolTipService::ToolTipProperty(), nullptr);
            Set(text, X::FrameworkElement::MarginProperty(),
                winrt::box_value(X::Thickness{menu == input_ ? 8. : 0., 0, 0, 0}));
            Set(text, X::FrameworkElement::VerticalAlignmentProperty(),
                winrt::box_value(X::VerticalAlignment::Center));
          } else if (!wcscmp(X::Automation::AutomationProperties::GetName(menu->button).c_str(),
                             L"Output device") &&
                     text.Text().size() == 1 && Parent(text).template try_as<C::Grid>() &&
                     Parent(text).template as<C::Grid>().ColumnDefinitions().Size() == 3) {
            Set(text, X::FrameworkElement::WidthProperty(), winrt::box_value(16.));
            Set(text, X::FrameworkElement::HeightProperty(), winrt::box_value(16.));
            Set(text, X::FrameworkElement::MarginProperty(),
                winrt::box_value(X::Thickness{0, 0, 8, 0}));
            Set(text, X::FrameworkElement::VerticalAlignmentProperty(),
                winrt::box_value(X::VerticalAlignment::Center));
            // Keep the native endpoint font glyph and its original binding.
            // No Viewbox scaling or guessed device-type replacement.
            auto parent = Parent(text).template as<C::Grid>();
            Set(parent.ColumnDefinitions().GetAt(0), C::ColumnDefinition::WidthProperty(),
                winrt::box_value(X::GridLengthHelper::FromPixels(24)));
            if (!Named(parent, L"NativeSoundEndpointFontIcon")) {
              auto glyph = SymbolGlyph(page_, text.Text()[0]);
              glyph.Name(L"NativeSoundEndpointFontIcon");
              glyph.FontFamily(text.FontFamily());
              glyph.Margin({0, listGlyphDrop, 8, -listGlyphDrop});
              D::Binding binding;
              binding.Source(text);
              binding.Path(X::PropertyPath(L"Text"));
              glyph.SetBinding(C::FontIcon::GlyphProperty(), binding);
              Add(parent, glyph);
              Set(text, X::UIElement::OpacityProperty(), winrt::box_value(0.));
            }
            if (item.IsSelected() && menu->endpointIcon)
              menu->endpointIcon.Glyph(text.Text());
          }
        }
        return false;
      });
    }
  }
  void PreparePicker(std::shared_ptr<Menu> const &menu) noexcept {
    try {
      CompactRealized(menu);
    } catch (...) {
      auto error = winrt::to_hresult();
      Wh_Log(L"Picker preparation: %08X", error);
    }
  }
  IInspectable RowModel(C::Slider const &slider) {
    for (auto e = Parent(slider); e && e != mixerList_; e = Parent(e))
      if (auto container = e.try_as<C::ListViewItem>()) {
        if (auto item = mixerList_.ItemFromContainer(container);
            item && item != X::DependencyProperty::UnsetValue())
          return item;
        break;
      }
    return VolumeModel(slider);
  }
  IInspectable ReadModel(IInspectable const &object, PCWSTR name) {
    if (auto value = ModelValue(object, name))
      return value;
    return access_.read ? access_.read(object, name) : nullptr;
  }
  bool WriteVolume(C::Slider const &slider, IInspectable const &object, double number) {
    try {
      if (SetNumber(object, L"Volume", number))
        return true;
    } catch (...) {
      Wh_Log(L"Model setter: %08X", winrt::to_hresult());
    }
    try {
      if (SendBoundNumber(slider, object, number))
        return true;
    } catch (...) {
      Wh_Log(L"Binding relay: %08X", winrt::to_hresult());
    }
    return access_.writeNumber && access_.writeNumber(object, L"Volume", number);
  }
  void ResizeNative() {
    if (!frame_ || !footerBorder_)
      return;
    auto viewport = Named(page_, L"ListContent").try_as<C::ScrollViewer>();
    if (!viewport)
      return;
    auto body = viewport.Content().try_as<X::FrameworkElement>();
    auto header = Named(page_, L"PageHeader");
    if (!body || !header)
      return;
    auto width = float(std::max(1., page_.ActualWidth()));
    body.Measure({width, INFINITY});
    footerBorder_.Measure({width, INFINITY});
    auto xamlRoot = page_.XamlRoot();
    double scale = xamlRoot ? xamlRoot.RasterizationScale() : 1.;
    MONITORINFO monitor{sizeof(monitor)};
    double cap = GetMonitorInfoW(MonitorFromWindow(host_, MONITOR_DEFAULTTONEAREST), &monitor)
                     ? (monitor.rcWork.bottom - monitor.rcWork.top) / scale - 24
                     : (xamlRoot ? xamlRoot.Size().Height - 24 : page_.ActualHeight());
    double content =
        header.ActualHeight() + body.DesiredSize().Height + footerBorder_.DesiredSize().Height;
    wantedHeight_ = std::min(std::floor(cap * scale) / scale, std::ceil(content * scale) / scale);
    auto target = TargetHeight();
    if (target > 0 &&
        (!std::isfinite(frame_.Height()) || std::abs(frame_.Height() - target) > .25)) {
      *ownedHeight_ = target;
      frame_.Height(target);
    }
  }
  // No custom value tooltip: the native Slider thumb tooltip is the only one
  // (IsThumbToolTipEnabled), as in the stock shell.
  void Add(C::Panel const &parent, X::UIElement const &child, uint32_t index = UINT_MAX) {
    parent.Children().InsertAt(std::min(index, parent.Children().Size()), child);
    journal_.undo.push_back([parent, child] {
      uint32_t i;
      if (parent.Children().IndexOf(child, i))
        parent.Children().RemoveAt(i);
    });
  }
  void Summary(std::shared_ptr<Menu> const &m) {
    auto selected = m->list.SelectedItem();
    std::wstring text;
    if (selected) {
      text = String(ModelValue(selected, L"EndpointFriendlyName"));
      if (text.empty())
        text = String(selected);
      if (text.empty())
        if (auto container = m->list.ContainerFromItem(selected).try_as<X::FrameworkElement>())
          if (auto title = Named(container, L"TitleText").try_as<C::TextBlock>())
            text = title.Text();
    }
    if (text.empty() && X::Automation::AutomationProperties::GetName(m->button) == L"Output device")
      for (auto const &device : latest_.output)
        if (device.current) {
          text = device.name;
          break;
        }
    if (m->endpointIcon &&
        X::Automation::AutomationProperties::GetName(m->button) == L"Output device") {
      auto code = String(ModelValue(selected, L"EndpointIcon"));
      if (code.size() == 1)
        m->endpointIcon.Glyph(code);
      else if (auto item = m->list.ContainerFromItem(selected).try_as<X::DependencyObject>())
        Find(item, [&](auto const &e) {
          if (auto glyph = e.template try_as<C::TextBlock>();
              glyph && glyph.Text().size() == 1 && C::Grid::GetColumn(glyph) == 0 &&
              Parent(glyph).template try_as<C::Grid>() &&
              Parent(glyph).template as<C::Grid>().ColumnDefinitions().Size() == 3) {
            m->endpointIcon.Glyph(glyph.Text());
            return true;
          }
          return false;
        });
    } // The native list can briefly lose its selection while the shell refreshes
    // endpoints; keep the last shown name instead of flashing a placeholder.
    if (text.empty())
      text = std::wstring(m->label.Text());
    m->label.Text(text.empty() ? L"—" : text);
    m->button.IsEnabled(m->list.IsEnabled() && m->list.Items().Size() > 0);
  }
  std::shared_ptr<Menu> Picker(X::FrameworkElement const &owner, PCWSTR name) {
    auto m = std::make_shared<Menu>();
    m->button = C::Button();
    m->button.Height(32);
    m->button.MinHeight(32);
    m->button.MinWidth(0);
    m->button.Padding({7, 0, 7, 0});
    m->button.Margin({20, 0, 20, 18});
    m->button.HorizontalAlignment(X::HorizontalAlignment::Stretch);
    m->button.HorizontalContentAlignment(X::HorizontalAlignment::Stretch);
    X::Automation::AutomationProperties::SetName(m->button, name);
    m->button.VerticalContentAlignment(X::VerticalAlignment::Center);
    auto g = Grid(L"17,*,12");
    if (!wcscmp(name, L"Spatial sound")) {
      auto phones = SymbolGlyph(owner, 0xe7f6, 16);
      phones.Margin({0, -buttonGlyphRaise, 0, buttonGlyphRaise});
      g.Children().Append(phones);
    } else {
      m->endpointIcon = SymbolGlyph(owner, !wcscmp(name, L"Input device") ? 0xe720 : 0xe7f5);
      m->endpointIcon.Margin({0, -buttonGlyphRaise, 0, buttonGlyphRaise});
      g.Children().Append(m->endpointIcon);
    }
    m->label = C::TextBlock();
    TextLine(m->label, 19.6);
    m->label.TextTrimming(X::TextTrimming::CharacterEllipsis);
    m->label.TextWrapping(X::TextWrapping::NoWrap);
    m->label.VerticalAlignment(X::VerticalAlignment::Center);
    m->label.Margin({8, 0, 8, 0});
    C::Grid::SetColumn(m->label, 1);
    g.Children().Append(m->label);
    m->arrow = SymbolGlyph(owner, 0xe70d, 12);
    C::Grid::SetColumn(m->arrow, 2);
    g.Children().Append(m->arrow);
    m->button.Content(g);
    m->flyout = C::Flyout();
    m->flyout.Placement(P::FlyoutPlacementMode::Bottom);
    m->button.Flyout(m->flyout);
    auto presenterStyle = X::Style(winrt::xaml_typename<C::FlyoutPresenter>());
    presenterStyle.Setters().Append(
        X::Setter(C::Control::PaddingProperty(), winrt::box_value(X::Thickness{4, 4, 4, 4})));
    presenterStyle.Setters().Append(
        X::Setter(X::FrameworkElement::MinWidthProperty(), winrt::box_value(0.)));
    presenterStyle.Setters().Append(X::Setter(C::Control::CornerRadiusProperty(),
                                              winrt::box_value(X::CornerRadius{8, 8, 8, 8})));
    m->flyout.FlyoutPresenterStyle(presenterStyle);
    Keep(m->flyout.Opening(winrt::auto_revoke, [this, m](auto &&, auto &&) {
      m->open = true;
      if (m != input_)
        CaptureMixer();
      Rotate(m->arrow, m->arrowStory, 180, 220);
      for (auto const &other : menus_)
        if (other != m)
          other->flyout.Hide();
      if (m->list) {
        auto root = m->button.XamlRoot();
        double scale = root ? root.RasterizationScale() : 1.;
        m->list.Width(std::max(32., m->button.ActualWidth() - 8 - 2 / scale));
        m->list.MaxHeight(300);
        if (m == input_)
          PreparePicker(m);
      }
    }));
    Keep(m->flyout.Opened(winrt::auto_revoke, [this, m](auto &&, auto &&) { PreparePicker(m); }));
    Keep(m->flyout.Closed(winrt::auto_revoke, [m](auto &&, auto &&) {
      m->open = false;
      Rotate(m->arrow, m->arrowStory, 0, 220);
    }));
    menus_.push_back(m);
    return m;
  }
  void Compact(C::ListView const &list) {
    journal_.Save(list, C::ItemsControl::ItemContainerStyleProperty());
    auto style = X::Style(winrt::xaml_typename<C::ListViewItem>());
    auto nativeStyle = Resource(page_, L"DefaultListViewItemStyle").try_as<X::Style>();
    style.BasedOn(pickerBase_   ? pickerBase_
                  : nativeStyle ? nativeStyle
                                : list.ItemContainerStyle());
    style.Setters().Append(
        X::Setter(X::FrameworkElement::MinHeightProperty(), winrt::box_value(32.)));
    style.Setters().Append(X::Setter(X::FrameworkElement::HeightProperty(), winrt::box_value(32.)));
    style.Setters().Append(
        X::Setter(C::Control::PaddingProperty(), winrt::box_value(X::Thickness{7, 0, 7, 0})));
    style.Setters().Append(X::Setter(C::Control::HorizontalContentAlignmentProperty(),
                                     winrt::box_value(X::HorizontalAlignment::Stretch)));
    style.Setters().Append(X::Setter(C::Control::VerticalContentAlignmentProperty(),
                                     winrt::box_value(X::VerticalAlignment::Center)));
    list.ItemContainerStyle(style);
    Set(list, C::Control::PaddingProperty(), winrt::box_value(X::Thickness{}));
    Set(list, X::FrameworkElement::MinWidthProperty(), winrt::box_value(0.));
    Set(list, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
    Set(list, C::ScrollViewer::HorizontalScrollModeProperty(),
        winrt::box_value(C::ScrollMode::Disabled));
    Set(list, C::ScrollViewer::HorizontalScrollBarVisibilityProperty(),
        winrt::box_value(C::ScrollBarVisibility::Disabled));
    Set(list, C::ScrollViewer::VerticalScrollModeProperty(),
        winrt::box_value(C::ScrollMode::Enabled));
    Set(list, C::ScrollViewer::VerticalScrollBarVisibilityProperty(),
        winrt::box_value(C::ScrollBarVisibility::Auto));
  }
  void ConvertList(C::ListView const &list, PCWSTR title) {
    auto parent = Parent(list).as<C::Panel>();
    uint32_t index = 0;
    if (!parent.Children().IndexOf(list, index))
      throw winrt::hresult_error(E_UNEXPECTED);
    auto m = Picker(parent, title);
    m->list = list;
    journal_.Save(list, X::FrameworkElement::DataContextProperty());
    list.DataContext(list.DataContext());
    journal_.Save(list, X::FrameworkElement::WidthProperty());
    journal_.Save(list, X::FrameworkElement::MaxHeightProperty());
    Compact(list);
    parent.Children().RemoveAt(index);
    m->flyout.Content(list);
    journal_.undo.push_back([parent, list, index, m] {
      m->flyout.Hide();
      m->flyout.Content(nullptr);
      parent.Children().InsertAt(std::min(index, parent.Children().Size()), list);
    });
    Add(parent, m->button, index);
    Keep(list.SelectionChanged(winrt::auto_revoke, [this, m](auto &&, auto &&) {
      Summary(m);
      if (m->open)
        m->flyout.Hide();
    }));
    Summary(m);
  }
  void Input(C::Panel const &parent) {
    inputBlock_ = C::StackPanel();
    inputBlock_.BorderThickness({0, 1, 0, 0});
    PaintDivider(inputBlock_, parent);
    C::TextBlock heading;
    heading.Text(L"Input device");
    heading.FontWeight({600});
    TextLine(heading, 19.6);
    heading.Margin({20, 10, 20, HeadingBottom()});
    inputBlock_.Children().Append(heading);
    input_ = Picker(parent, L"Input device");
    input_->list = C::ListView();
    input_->list.SelectionMode(C::ListViewSelectionMode::Single);
    input_->list.IsItemClickEnabled(true);
    input_->list.HorizontalAlignment(X::HorizontalAlignment::Stretch);
    C::ScrollViewer::SetHorizontalScrollBarVisibility(input_->list,
                                                      C::ScrollBarVisibility::Disabled);
    Compact(input_->list);
    input_->flyout.Content(input_->list);
    inputBlock_.Children().Append(input_->button);
    Add(parent, inputBlock_);
    // Keep the native flyout animation; newly created input rows must not run
    // independent entrance/reposition transitions inside that moving surface.
    input_->list.ItemContainerTransitions(X::Media::Animation::TransitionCollection());
    Keep(input_->list.ItemClick(
        winrt::auto_revoke,
        [this](auto &&, C::ItemClickEventArgs const &args) { ChooseInput(args); }));
  }
  void ChooseInput(C::ItemClickEventArgs const &args) noexcept {
    try {
      auto clicked = args.ClickedItem();
      std::wstring id;
      // With explicit ListViewItems, ClickedItem can be their content Grid.
      // Resolve only the clicked object/container, never the previous selection.
      if (auto element = clicked.try_as<X::FrameworkElement>()) {
        id = String(element.Tag());
        if (id.empty())
          if (auto container = input_->list.ContainerFromItem(clicked).try_as<C::ListViewItem>())
            id = String(container.Tag());
        for (auto node = element; id.empty() && node && node != input_->list; node = Parent(node))
          if (auto item = node.try_as<C::ListViewItem>())
            id = String(item.Tag());
      }
      if (id.empty())
        throw winrt::hresult_error(E_INVALIDARG, L"Clicked microphone has no endpoint ID");
      pendingInputId_ = id;
      pendingInput_ = audio_.Send({sound::Action::Input, id});
      input_->flyout.Hide();
    } catch (...) {
      auto error = winrt::to_hresult();
      Wh_Log(L"Input selection: %08X", error);
    }
  }
  void SyncInput(sound::Snapshot const &state) {
    // Selection changes do not change the collection. Replacing explicit XAML
    // containers during ItemClick/closing can invalidate the routed input path.
    std::wstring signature;
    for (auto const &device : state.input)
      signature += std::to_wstring(device.id.size()) + L":" + device.id +
                   std::to_wstring(device.name.size()) + L":" + device.name;
    if (signature != inputSignature_ && !input_->open) {
      input_->list.Items().Clear();
      for (auto const &device : state.input) {
        C::ListViewItem item;
        item.Style(input_->list.ItemContainerStyle());
        item.Tag(winrt::box_value(device.id));
        item.ContentTransitions(X::Media::Animation::TransitionCollection());
        item.Transitions(X::Media::Animation::TransitionCollection());
        auto grid = Grid(L"16,*");
        grid.Tag(winrt::box_value(device.id));
        {
          auto mic = SymbolGlyph(page_, 0xe720);
          mic.Margin({0, listGlyphDrop, 0, -listGlyphDrop});
          grid.Children().Append(mic);
        }
        C::TextBlock label;
        label.Name(L"TitleText");
        label.Text(device.name);
        label.TextTrimming(X::TextTrimming::CharacterEllipsis);
        label.TextWrapping(X::TextWrapping::NoWrap);
        label.VerticalAlignment(X::VerticalAlignment::Center);
        label.FontSize(14);
        TextLine(label, 19);
        label.Margin({8, 0, 0, 0});
        C::Grid::SetColumn(label, 1);
        grid.Children().Append(label);
        item.Content(grid);
        input_->list.Items().Append(item);
      }
      CompactRealized(input_);
      inputSignature_ = signature;
    }
    C::ListViewItem selected{nullptr};
    for (auto const &device : state.input)
      if (device.current) {
        input_->label.Text(device.name);
        for (auto const &entry : input_->list.Items())
          if (auto item = entry.try_as<C::ListViewItem>();
              item && String(item.Tag()) == device.id) {
            selected = item;
            break;
          }
        break;
      }
    if (selected && input_->list.SelectedItem() != selected)
      input_->list.SelectedItem(selected);
    input_->button.IsEnabled(!state.input.empty());
    if (pendingInput_ && state.applied >= pendingInput_) {
      pendingInput_ = 0;
    }
  }
  void Mixer() {
    auto title = Named(page_, L"MixerGroupTitle").as<C::TextBlock>();
    auto header = Parent(title).as<C::Panel>();
    uint32_t index;
    if (!header.Children().IndexOf(title, index))
      throw winrt::hresult_error(E_UNEXPECTED);
    if (auto block = Parent(header).try_as<C::StackPanel>())
      Set(block, C::StackPanel::PaddingProperty(), winrt::box_value(X::Thickness{0, 0, 0, 11}));
    // IconButtonStyle supplies a symbol font. Freeze the native text typography
    // while the heading is still in its original parent, before reparenting it.
    auto titleFont = title.FontFamily();
    auto titleSize = title.FontSize();
    Set(title, C::TextBlock::FontFamilyProperty(), titleFont);
    Set(title, C::TextBlock::FontSizeProperty(), winrt::box_value(titleSize));
    TextLine(title, 19.6);
    mixerButton_ = C::Button();
    mixerButton_.MinWidth(0);
    mixerButton_.MinHeight(32);
    mixerButton_.Height(32);
    mixerButton_.Padding({8, 0, 8, 0});
    mixerButton_.Margin({12, 11, 52, 11});
    mixerButton_.HorizontalAlignment(X::HorizontalAlignment::Left);
    mixerButton_.HorizontalContentAlignment(X::HorizontalAlignment::Stretch);
    if (auto style = Resource(page_, L"IconButtonStyle").try_as<X::Style>())
      mixerButton_.Style(style);
    mixerButton_.FontFamily(titleFont);
    mixerButton_.FontSize(titleSize);
    mixerButton_.Width(NAN);
    mixerButton_.Height(32);
    mixerButton_.MinHeight(32);
    mixerButton_.MinWidth(0);
    mixerButton_.Padding({8, 0, 8, 0});
    mixerButton_.Margin({12, 11, 52, 0});
    mixerButton_.VerticalContentAlignment(X::VerticalAlignment::Center);
    mixerButton_.HorizontalContentAlignment(X::HorizontalAlignment::Stretch);
    journal_.Save(title, X::FrameworkElement::MarginProperty());
    title.Margin({0, 0, 8, 0}); // the chevron 8 DIP after the title; the plate hugs both
    auto g = Grid(L"Auto,12");
    header.Children().RemoveAt(index);
    g.Children().Append(title);
    arrow_ = SymbolGlyph(page_, 0xe70d, 12);
    C::Grid::SetColumn(arrow_, 1);
    g.Children().Append(arrow_);
    mixerButton_.Content(g);
    journal_.undo.push_back([header, title, index, g] {
      uint32_t i;
      if (g.Children().IndexOf(title, i))
        g.Children().RemoveAt(i);
      header.Children().InsertAt(std::min(index, header.Children().Size()), title);
    });
    Add(header, mixerButton_, index);
    if (auto settings = Find(header, [](auto const &e) {
                          auto button = e.template try_as<C::Button>();
                          return button &&
                                 String(button.CommandParameter()) == L"ms-settings:apps-volume";
                        }).try_as<C::Button>()) {
      journal_.Save(settings, X::FrameworkElement::StyleProperty());
      if (auto iconStyle = Resource(page_, L"IconButtonStyle").try_as<X::Style>())
        settings.Style(iconStyle);
      for (auto const &property :
           {X::FrameworkElement::WidthProperty(), X::FrameworkElement::HeightProperty(),
            X::FrameworkElement::MinWidthProperty(), X::FrameworkElement::MinHeightProperty(),
            X::FrameworkElement::MarginProperty(), C::Control::PaddingProperty(),
            C::ContentControl::ContentProperty()})
        journal_.Save(settings, property);
      settings.Width(32);
      settings.Height(32);
      settings.MinWidth(0);
      settings.MinHeight(0);
      settings.Padding({0, 0, 0, 0});
      settings.Margin({0, 11, 20, 0});
      settings.Content(Icon(L"sliders-horizontal"));
    }
    journal_.Save(mixerList_, X::UIElement::VisibilityProperty());
    journal_.Save(mixerList_, X::FrameworkElement::HeightProperty());
    journal_.Save(mixerList_, X::UIElement::OpacityProperty());
    // The VM's 220ms expand/collapse animation owns the whole list. Individual
    // entrance/reposition transitions would continue after the preflight pass.
    Set(mixerList_, C::ItemsControl::ItemContainerTransitionsProperty(),
        X::Media::Animation::TransitionCollection());
    // First Sound page in this shell process opens with VM collapsed: the shell
    // loads mixer rows last and asynchronously. The saved state is not changed
    // and applies from the next opening; a click saves the new state as usual.
    expanded_ = soundOpenedOnce && Wh_GetIntValue(L"nativeMixerExpanded", 1) != 0;
    soundOpenedOnce = true;
    ApplyExpanded(false);
    Keep(mixerButton_.Click(winrt::auto_revoke, [this](auto &&, auto &&) {
      expanded_ = !expanded_;
      Wh_SetIntValue(L"nativeMixerExpanded", expanded_);
      ApplyExpanded(true);
    }));
    journal_.Save(mixerList_, C::ItemsControl::ItemContainerStyleProperty());
    auto itemStyle = X::Style(winrt::xaml_typename<C::ListViewItem>());
    itemStyle.BasedOn(mixerList_.ItemContainerStyle());
    itemStyle.Setters().Append(
        X::Setter(C::Control::PaddingProperty(), winrt::box_value(X::Thickness{})));
    itemStyle.Setters().Append(
        X::Setter(X::FrameworkElement::MinHeightProperty(), winrt::box_value(38.)));
    itemStyle.Setters().Append(
        X::Setter(X::FrameworkElement::HeightProperty(), winrt::box_value(38.)));
    itemStyle.Setters().Append(X::Setter(C::Control::HorizontalContentAlignmentProperty(),
                                         winrt::box_value(X::HorizontalAlignment::Stretch)));
    mixerList_.ItemContainerStyle(itemStyle);
    Set(mixerList_, C::Control::PaddingProperty(), winrt::box_value(X::Thickness{}));
    Set(mixerList_, X::FrameworkElement::MarginProperty(), winrt::box_value(X::Thickness{}));
  }
  void ApplyExpanded(bool animate) {
    if (holding_) {
      holding_ = false;
      if (holdTimer_)
        holdTimer_.Stop();
    }
    holdAnimation_ = false;
    ClearSnapshot();
    if (animation_)
      animation_.Stop();
    auto rotation = arrow_.RenderTransform().try_as<M::RotateTransform>();
    if (!rotation) {
      rotation = M::RotateTransform();
      arrow_.RenderTransformOrigin({.5f, .5f});
      arrow_.RenderTransform(rotation);
    }
    arrowStart_ = rotation.Angle();
    X::Automation::AutomationProperties::SetHelpText(mixerButton_,
                                                     expanded_ ? L"Expanded" : L"Collapsed");
    if (!animate || !winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
      hideUnnamed_ = false;
      rotation.Angle(expanded_ ? 180 : 0);
      mixerList_.Height(NAN);
      mixerList_.Opacity(1);
      mixerList_.Visibility(expanded_ ? X::Visibility::Visible : X::Visibility::Collapsed);
      return;
    }
    hideUnnamed_ = expanded_;
    from_ = mixerList_.Visibility() == X::Visibility::Visible ? mixerList_.ActualHeight() : 0;
    mixerList_.Visibility(X::Visibility::Visible);
    mixerList_.Height(NAN);
    mixerList_.Measure({float(std::max(1., mixerList_.ActualWidth())), INFINITY});
    // Rows realized by this measure (first expansion after a collapsed opening)
    // get hidden apps and 38 DIP heights before the target height is taken.
    if (expanded_)
      try {
        PrepareMixerRows();
        AlignTextBaselines();
        mixerList_.Measure({float(std::max(1., mixerList_.ActualWidth())), INFINITY});
      } catch (...) {
        Wh_Log(L"ExpandPrepareError %08X", winrt::to_hresult());
      }
    to_ = expanded_ ? mixerList_.DesiredSize().Height : 0;
    mixerList_.Height(from_);
    start_ = NowMs();
    animFirst_ = true;
    EnsureAnimation();
    animation_.Start();
    StartAnimationLoop();
  }
  static double NowMs() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return double(c.QuadPart) * 1000. / double(f.QuadPart);
  }
  std::function<void()> animationFrame_;
  bool animFirst_ = false, renderingOn_ = false;
  double frameStart_ = NAN;
  winrt::event_token renderingToken_{};
  void StartAnimationLoop() {
    if (renderingOn_)
      return;
    renderingOn_ = true;
    renderingToken_ = M::CompositionTarget::Rendering([this](auto &&, auto &&) {
      try {
        if (!animation_ || !animation_.IsEnabled()) {
          StopAnimationLoop();
          return;
        }
        if (animationFrame_)
          animationFrame_();
      } catch (...) {
        Wh_Log(L"AnimationFrameError %08X", winrt::to_hresult());
      }
    });
  }
  void StopAnimationLoop() {
    if (!renderingOn_)
      return;
    renderingOn_ = false;
    try {
      M::CompositionTarget::Rendering(renderingToken_);
    } catch (...) {
    }
  }
  void EnsureAnimation() {
    if (animation_)
      return;
    // The timer only marks the animation as running; frames come from
    // CompositionTarget.Rendering (one per rendered frame instead of a 16 ms
    // DispatcherTimer beating against vsync), time from QPC (GetTickCount64
    // moves in ~16 ms steps). The panel height and the list's natural height are
    // measured every frame: the first natural height is measured before the rows
    // are re-styled, so a height derived once overshoots.
    animation_ = X::DispatcherTimer();
    animation_.Interval(std::chrono::hours(1));
    animationTick_ = animation_.Tick([](auto &&, auto &&) {});
    animationFrame_ = [this] {
      auto t = std::min(1., (NowMs() - start_) / 220.);
      auto eased = PanelEase(t);
      if (animFirst_) {
        animFirst_ = false;
        frameStart_ = frame_ && std::isfinite(frame_.Height()) ? frame_.Height() : wantedHeight_;
      }
      // The shell may still add/reorder sessions while the list expands:
      // prepare new rows and re-measure the natural target on every tick.
      // A rebuild seen by the timer before LayoutUpdated must still be held.
      if (holdAnimation_ && expanded_ && t < 1)
        try {
          if (IsRebuild(rowItems_, RowItems())) {
            StartHold();
            return;
          }
        } catch (...) {
        }
      if (expanded_ && t < 1)
        try {
          if (rowsDirty_ || RowSignature() != rowSignature_) {
            PrepareMixerRows();
            AlignTextBaselines();
          }
          mixerList_.Height(NAN);
          mixerList_.Measure({float(std::max(1., mixerList_.ActualWidth())), INFINITY});
          to_ = mixerList_.DesiredSize().Height;
        } catch (...) {
        }
      mixerList_.Height(from_ + (to_ - from_) * eased);
      arrow_.RenderTransform().as<M::RotateTransform>().Angle(
          arrowStart_ + ((expanded_ ? 180. : 0.) - arrowStart_) * eased);
      if (crossfade_) {
        auto f = std::min(1., t * 220 / 160);
        mixerList_.Opacity(1);
        if (itemsHost_)
          itemsHost_.Opacity(f);
        if (snapshotBrush_)
          snapshotBrush_.Opacity(1 - f);
      } else
        mixerList_.Opacity(expanded_ ? std::min(1., t * 220 / 160)
                                     : std::max(0., 1 - t * 220 / 160));
      if (t >= 1) {
        animation_.Stop();
        hideUnnamed_ = false;
        holdAnimation_ = false;
        ClearSnapshot();
        ScheduleRowsRecheck();
        mixerList_.Height(NAN);
        mixerList_.Opacity(1);
        mixerList_.Visibility(expanded_ ? X::Visibility::Visible : X::Visibility::Collapsed);
      }
      ResizeNative();
    };
  }
  void PrepareMixerRows() {
    SliderGeometry(master_);
    auto style = Resource(page_, L"HeaderIconSliderStyle").try_as<X::Style>();
    std::vector<C::Slider> found;
    Find(mixerList_, [&](auto const &e) {
      if (auto s = e.template try_as<C::Slider>(); s && s.Name() == L"AppVolumeLevel")
        found.push_back(s);
      return false;
    });
    for (auto const &s : found)
      App(s, style);
    bool pending = false;
    for (auto const &row : rows_)
      if (auto root = row->root.get()) {
        auto slider = row->slider.get();
        if (slider && RowModel(slider) != row->model) {
          row->model = RowModel(slider);
          row->last = slider.Value() > 0 ? slider.Value() : 50;
          row->beforeValid = false;
          row->expected = NAN;
          row->unnamedSince = 0;
        }
        // AppName once per row model (it was read on every pass, dozens of times per
        // opening). Re-read only while the binding has not resolved it yet.
        std::wstring name;
        if (row->nameModel == row->model && !row->name.empty())
          name = row->name;
        else {
          name = String(ReadModel(row->model, L"AppName"));
          row->name = name;
          row->nameModel = row->model;
        }
        if (name.empty())
          name = root.Name();
        // A new row whose binding has not resolved yet cannot be matched against
        // hiddenApps; keep it transparent (max 500 ms) instead of showing it.
        bool unnamed = name.empty();
        if (unnamed) {
          auto now = GetTickCount64();
          if (!row->unnamedSince)
            row->unnamedSince = now;
          if (now - row->unnamedSince < 500)
            pending = true;
          else
            unnamed = false;
        } else
          row->unnamedSince = 0;
        X::FrameworkElement container = root;
        for (auto p = Parent(root); p && p != mixerList_; p = Parent(p))
          if (p.try_as<C::ListViewItem>()) {
            container = p;
            break;
          }
        if (auto item = container.try_as<C::ListViewItem>()) {
          Set(item, X::UIElement::TransitionsProperty(),
              X::Media::Animation::TransitionCollection());
          Set(item, C::ContentControl::ContentTransitionsProperty(),
              X::Media::Animation::TransitionCollection());
        }
        journal_.Save(container, X::UIElement::VisibilityProperty());
        container.Visibility(sound::Hidden(hidden, name, L"") ? X::Visibility::Collapsed
                                                              : X::Visibility::Visible);
        journal_.Save(container, X::UIElement::OpacityProperty());
        container.Opacity(unnamed && hideUnnamed_ ? 0 : 1);
        if (slider && std::isfinite(row->expected) && GetTickCount64() >= row->verifyAt) {
          row->expected = NAN;
        }
      }
    AlignRows();
    rowsDirty_ = false;
    rowSignature_ = RowSignature();
    rowItems_ = RowItems();
    pendingRows_ = pending;
    // Unresolved rows are re-checked by a timer on the next frame. Keeping
    // rowsDirty_ set made every LayoutUpdated re-layout: XAML layout cycle,
    // ShellHost 0xc000027b in Windows.UI.Xaml.dll when Spatial sound changed.
    if (pending)
      ScheduleRowsRecheck();
  }
  double PanelCap() const {
    auto root = page_.XamlRoot();
    double scale = root ? root.RasterizationScale() : 1.;
    MONITORINFO monitor{sizeof(monitor)};
    double cap = GetMonitorInfoW(MonitorFromWindow(host_, MONITOR_DEFAULTTONEAREST), &monitor)
                     ? (monitor.rcWork.bottom - monitor.rcWork.top) / scale - 24
                     : (root ? root.Size().Height - 24 : page_.ActualHeight());
    return std::floor(cap * scale) / scale;
  }
  void SetPanelHeight(double value) {
    if (frame_ && value > 0) {
      *ownedHeight_ = value;
      frame_.Height(value);
    }
  }
  void Master(C::ItemsControl const &oldFooter, X::Style const &sliderStyle) {
    // Keep the actual Footer and its native group/border visible. The shell uses
    // that slot when measuring the advanced page; a sibling is not equivalent.
    footerBorder_ = Find(oldFooter, [](auto const &e) {
                      auto border = e.template try_as<C::Border>();
                      return border && border.BorderThickness().Top > 0;
                    }).as<C::Border>();
    // The SV/VM divider in the colour of the Quick Settings block divider.
    {
      journal_.Save(footerBorder_, C::Border::BorderBrushProperty());
      journal_.Save(footerBorder_, C::Border::BorderThicknessProperty());
      // 1 DIP like the other dividers (one pixel at 125%).
      footerBorder_.BorderThickness({0, 1, 0, 0});
      FooterLine();
      Keep(footerBorder_.ActualThemeChanged(winrt::auto_revoke, [this](auto &&, auto &&) {
        try {
          if (footerBorder_)
            FooterLine();
          if (inputBlock_)
            PaintDivider(inputBlock_, page_);
        } catch (...) {
        }
      }));
    }
    auto original = footerBorder_.Child();
    footer_ = Grid(L"32,*,32");
    footer_.Name(L"NativeSoundMasterRow");
    FooterMargins();
    masterIcon_ = Button(page_, L"Mute system volume");
    masterIcon_.Content(VolumeGlyph(page_, 3));
    try {
      masterAnimation_ = std::make_shared<AnimatedVolume>();
      masterIcon_.Content(masterAnimation_->Isolated());
    } catch (...) {
      Wh_Log(L"MasterAnimationError %08X", winrt::to_hresult());
    }
    footer_.Children().Append(masterIcon_);
    master_ = C::Slider();
    master_.Style(sliderStyle);
    master_.MinWidth(0);
    master_.MinHeight(0);
    master_.Height(32);
    master_.Margin({12, 0, 12, 0});
    master_.Maximum(100);
    master_.StepFrequency(1);
    master_.IsThumbToolTipEnabled(true);
    C::Grid::SetColumn(master_, 1);
    footer_.Children().Append(master_);
    X::Automation::AutomationProperties::SetName(master_, L"System volume");
    auto settings = Button(page_, L"More volume settings");
    SettingsGear(settings);
    C::Grid::SetColumn(settings, 2);
    footer_.Children().Append(settings);
    auto nativeBorder = footerBorder_;
    nativeBorder.Child(footer_);
    journal_.undo.push_back([nativeBorder, original] { nativeBorder.Child(original); });
    Keep(settings.Click(winrt::auto_revoke, [](auto &&, auto &&) {
      ShellExecuteW(nullptr, L"open", L"ms-settings:sound", nullptr, nullptr, SW_SHOWNORMAL);
    }));
    Keep(master_.ValueChanged(winrt::auto_revoke, [this](auto &&, auto const &args) {
      if (!sync_) {
        userChangeAt_ = GetTickCount64();
        auto value = float(args.NewValue() / 100);
        pending_ = audio_.Send({sound::Action::MasterVolume, L"", value});
        MasterGlyph(value, value <= 0);
      }
    }));
    Keep(master_.PointerWheelChanged(
        winrt::auto_revoke, [this](auto &&, X::Input::PointerRoutedEventArgs const &args) {
          try {
            if (args.Handled() || !master_.IsEnabled())
              return;
            auto point = args.GetCurrentPoint(master_);
            if (point.Properties().IsHorizontalMouseWheel())
              return;
            double delta = point.Properties().MouseWheelDelta() / 120.;
            if (!delta)
              return;
            master_.Value(std::clamp(master_.Value() + delta * master_.SmallChange(),
                                     master_.Minimum(), master_.Maximum()));
            args.Handled(true);
          } catch (...) {
            Wh_Log(L"MasterWheelError %08X", winrt::to_hresult());
          }
        }));
    Keep(masterIcon_.Click(winrt::auto_revoke, [this](auto &&, auto &&) {
      pending_ = audio_.Send({sound::Action::MasterMute});
    }));
  }

public:
  explicit SoundPage(X::FrameworkElement const &page, HWND host, ModelAccess access,
                     std::function<void()> volumeChanged)
      : page_(page), host_(host), access_(std::move(access)),
        volumeChanged_(std::move(volumeChanged)) {}
  void MasterGlyph(float value, bool muted) {
    if (masterAnimation_)
      try {
        masterAnimation_->Update(value, muted);
        return;
      } catch (...) {
        Wh_Log(L"MasterAnimationError %08X", winrt::to_hresult());
        masterAnimation_.reset();
        glyph_ = -1;
      }
    int glyph = sound::VolumeIcon(value, muted);
    if (glyph != glyph_) {
      glyph_ = glyph;
      masterIcon_.Content(VolumeGlyph(page_, glyph));
    }
  }
  void RefreshMasterNow() {
    if (master_ && page_.IsLoaded())
      ApplyMaster(State());
  }
  sound::Snapshot State() {
    auto s = audio_.Get();
    if (s.revision) {
      lastAudio = s;
      return s;
    }
    if (lastAudio.revision) {
      auto c = lastAudio;
      c.applied = s.applied;
      c.error = s.error;
      c.masterNoticeAt = 0;
      return c;
    }
    return s;
  }
  void ApplyMaster(sound::Snapshot const &state) {
    if (!master_ || state.applied < pending_)
      return;
    // While the user moves SV, the echo of an earlier value from Core Audio is
    // not written back into the slider (it pulled the thumb back between drag
    // steps).
    bool dragging = GetTickCount64() - userChangeAt_ < 400;
    sync_ = true;
    master_.IsEnabled(state.masterAvailable);
    masterIcon_.IsEnabled(state.masterAvailable);
    if (!dragging)
      master_.Value(state.muted ? 0 : std::round(state.master * 100));
    sync_ = false;
    if (!dragging)
      MasterGlyph(state.master, state.muted);
  }
  X::FrameworkElement Page() const { return page_; }
  bool Alive() const {
    return page_ && page_.IsLoaded() && outputAnchor_ && outputAnchor_.IsLoaded() &&
           Named(page_, L"OutputGroupTitle") == outputAnchor_;
  }
  bool Apply() {
    auto outputTitle = Named(page_, L"OutputGroupTitle");
    auto output = Named(page_, L"ListWithOutputGroupTitle").try_as<C::ListView>();
    auto spatialTitle = Named(page_, L"SpatialGroupTitle");
    auto mixer = Named(page_, L"MixerGroupTitle");
    auto oldFooter = Named(page_, L"Footer").try_as<C::ItemsControl>();
    auto style = Resource(page_, L"HeaderIconSliderStyle").try_as<X::Style>();
    if (!outputTitle || !output || !spatialTitle || !mixer || !oldFooter || !style)
      return false;
    auto spatial = Find(Parent(spatialTitle), [](auto const &e) {
                     return bool(e.template try_as<C::ListView>());
                   }).try_as<C::ListView>();
    auto mixerRoot = Parent(Parent(mixer));
    mixerList_ = Find(mixerRoot, [](auto const &e) {
                   return bool(e.template try_as<C::ListView>());
                 }).try_as<C::ListView>();
    if (!spatial || !mixerList_ || !Parent(output).try_as<C::StackPanel>() ||
        !Find(oldFooter, [](auto const &e) {
          auto border = e.template try_as<C::Border>();
          return border && border.BorderThickness().Top > 0;
        }))
      return false;
    outputAnchor_ = outputTitle;
    if (auto original = output.ItemContainerStyle())
      pickerBase_ = original.BasedOn();
    for (auto heading : {outputTitle, spatialTitle, mixer})
      NormalizeGroup(heading);
    for (auto name : {L"PageTitleText", L"PageTitle"})
      if (!pageTitle_)
        pageTitle_ = Named(page_, name).try_as<C::TextBlock>();
    for (auto heading : {outputTitle, spatialTitle}) {
      journal_.Save(heading, X::FrameworkElement::MarginProperty());
      heading.Margin({20, 10, 20, HeadingBottom()});
      TextLine(heading.as<C::TextBlock>(), 19.6);
    }
    ConvertList(output, L"Output device");
    Input(Parent(outputTitle).as<C::Panel>());
    ConvertList(spatial, L"Spatial sound");
    Mixer();
    Master(oldFooter, style);
    if (auto back = Named(page_, L"BackButton").try_as<C::Button>()) {
      Set(back, X::FrameworkElement::WidthProperty(), winrt::box_value(32.));
      Set(back, X::FrameworkElement::HeightProperty(), winrt::box_value(32.));
      Set(back, X::FrameworkElement::MinWidthProperty(), winrt::box_value(0.));
      Set(back, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
      Set(back, X::FrameworkElement::MarginProperty(),
          winrt::box_value(X::Thickness{16, 0, 12, 0}));
      // Hiding the page (ExitWithSnapshot): its command is wrapped; weak references only,
      // an abandoned page keeps the wrapper.
      if (auto original = back.Command()) {
        auto weakPage = winrt::make_weak(page_);
        Set(back, P::ButtonBase::CommandProperty(),
            winrt::make<DelayedCommand>(original, [weakPage, original](IInspectable const &p) {
              auto page = weakPage.get();
              X::FrameworkElement frame{nullptr};
              if (page)
                for (auto e = Parent(page); e; e = Parent(e))
                  if (e.Name() == L"L2Frame") {
                    frame = e;
                    break;
                  }
              if (!page || !frame) {
                original.Execute(p);
                return;
              }
              Leave(frame, page, original, p);
            }));
      } else {
        // BackButton has no Command (FullScreenPage.xaml: the shell handles Click
        // in code), and its handler runs before any added one, so the page would be
        // gone before a snapshot. A clone with the same style and glyph takes the
        // clicks; the original stays in place (transparent, no hit testing, not a
        // tab stop) and is clicked through UI Automation once the snapshot is laid
        // over the panel.
        if (auto parent = Parent(back).try_as<C::Panel>()) {
          C::Button clone;
          clone.Style(back.Style());
          clone.Width(back.Width());
          clone.Height(back.Height());
          clone.MinWidth(0);
          clone.MinHeight(0);
          clone.Margin(back.Margin());
          clone.Padding(back.Padding());
          clone.HorizontalAlignment(back.HorizontalAlignment());
          clone.VerticalAlignment(back.VerticalAlignment());
          C::Grid::SetColumn(clone, C::Grid::GetColumn(back));
          C::Grid::SetRow(clone, C::Grid::GetRow(back));
          if (auto icon = back.Content().try_as<C::FontIcon>()) {
            C::FontIcon copy;
            copy.Glyph(icon.Glyph());
            copy.FontSize(icon.FontSize());
            copy.FontFamily(icon.FontFamily());
            copy.MirroredWhenRightToLeft(icon.MirroredWhenRightToLeft());
            clone.Content(copy);
          }
          auto name = X::Automation::AutomationProperties::GetName(back);
          X::Automation::AutomationProperties::SetName(clone, name);
          if (auto tip = C::ToolTipService::GetToolTip(back))
            C::ToolTipService::SetToolTip(clone, tip);
          Set(back, X::UIElement::OpacityProperty(), winrt::box_value(0.));
          Set(back, X::UIElement::IsHitTestVisibleProperty(), winrt::box_value(false));
          Set(back, C::Control::IsTabStopProperty(), winrt::box_value(false));
          uint32_t index = 0;
          parent.Children().IndexOf(back, index);
          Add(parent, clone, index + 1);
          auto weakBack = winrt::make_weak(back);
          auto weakPage = winrt::make_weak(page_);
          Keep(clone.Click(winrt::auto_revoke, [weakBack, weakPage](auto &&, auto &&) {
            auto press = [weakBack] {
              try {
                if (auto b = weakBack.get())
                  if (auto peer = X::Automation::Peers::FrameworkElementAutomationPeer::
                          CreatePeerForElement(b))
                    if (auto invoke =
                            peer.GetPattern(X::Automation::Peers::PatternInterface::Invoke)
                                .try_as<X::Automation::Provider::IInvokeProvider>())
                      invoke.Invoke();
              } catch (...) {
                Wh_Log(L"BackInvokeError %08X", winrt::to_hresult());
              }
            };
            // L2Frame is looked up at click time: Apply finds frame_ only after this
            // handler is created.
            auto page = weakPage.get();
            X::FrameworkElement frame{nullptr};
            if (page)
              for (auto e = Parent(page); e; e = Parent(e))
                if (e.Name() == L"L2Frame") {
                  frame = e;
                  break;
                }
            if (page && frame &&
                winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
              ExitWithSnapshot(frame, page, press);
              return;
            }
            press();
          }));
        }
      }
    }
    for (auto name : {L"PageTitleText", L"PageTitle"})
      if (auto t = Named(page_, name).try_as<C::TextBlock>()) {
        journal_.Save(t, C::TextBlock::TextProperty());
        t.Text(L"Sound");
        TextLine(t, 19.6);
      }
    journal_.Save(page_, X::FrameworkElement::HeightProperty());
    page_.Height(NAN);
    for (auto e = Parent(page_); e; e = Parent(e))
      if (e.Name() == L"L2Frame") {
        frame_ = e;
        break;
      }
    if (frame_) {
      frameJournal_.Save(frame_, X::FrameworkElement::MinHeightProperty());
      frame_.MinHeight(0);
      auto property = X::FrameworkElement::HeightProperty();
      auto original = frame_.ReadLocalValue(property);
      D::BindingBase binding{nullptr};
      if (auto expression = frame_.GetBindingExpression(property))
        binding = expression.ParentBinding();
      auto frame = frame_;
      auto owned = ownedHeight_;
      frameJournal_.undo.push_back([frame, owned, original, property, binding] {
        if (std::isfinite(*owned) && std::abs(frame.Height() - *owned) < .25) {
          if (binding)
            D::BindingOperations::SetBinding(frame, property, binding);
          else if (original == X::DependencyProperty::UnsetValue())
            frame.ClearValue(property);
          else
            frame.SetValue(property, original);
        }
      });
    }
    audio_.Start(volumeChanged_);
    return true;
  }
  void PrepareInitialLayout() {
    if (preparing_ || !frame_)
      return;
    preparing_ = true;
    try {
      // The VM list keeps its saved Visibility during preparation: collapsing and
      // re-expanding it here forces extra layout passes and shows native rows with
      // zero volume/mute and no app icons. The other VM state is measured by
      // ApplyExpanded only when the user toggles it. No forced
      // page_.UpdateLayout() here: it costs hundreds of milliseconds. Rows,
      // baselines and Frame height are applied in the first LayoutUpdated, after
      // the shell's own layout pass and before that frame is rendered.
      ApplyExpanded(false);
      auto state = State();
      latest_ = state;
      SyncInput(state);
      ApplyMaster(state);
      for (auto const &menu : menus_)
        if (menu != input_)
          Summary(menu);
      rowsDirty_ = true;
      prepared_ = true;
      preparing_ = false;
      if (!DataReady(false))
        StartGate();
      RememberHeight();
      StartOpenHold();
      FooterMargins();
    } catch (...) {
      preparing_ = false;
      throw;
    }
  }
  // First opening in a shell session: the native view models load asynchronously
  // ("No output devices found", empty Spatial/VM, then the panel grows). Keep the
  // page content transparent until the data and VM rows are present, max 300 ms.
  std::shared_ptr<Menu> MenuNamed(PCWSTR name) {
    for (auto const &m : menus_)
      if (X::Automation::AutomationProperties::GetName(m->button) == name)
        return m;
    return nullptr;
  }
  bool DataReady(bool rows) {
    auto out = MenuNamed(L"Output device"), spatial = MenuNamed(L"Spatial sound");
    if (!out || !out->list.Items().Size() || !out->list.SelectedItem())
      return false;
    // The shell first selects a provisional endpoint (seen: Steam Streaming
    // Microphone with Spatial "Off"); wait until it is Core Audio's default.
    auto state = State();
    std::wstring current;
    for (auto const &d : state.output)
      if (d.current) {
        current = d.name;
        break;
      }
    if (current.empty())
      return false;
    if (auto name = String(ModelValue(out->list.SelectedItem(), L"EndpointFriendlyName"));
        !name.empty() && name != current)
      return false;
    if (spatial && !spatial->list.Items().Size())
      return false;
    if (!expanded_)
      return true;
    auto n = mixerList_.Items().Size();
    if (!n)
      return false;
    return !rows || (!rowsDirty_ && mixerList_.ContainerFromIndex(n - 1));
  }
  // While the content is hidden the empty surface keeps one height: the last
  // shown height for this VM state (persisted), instead of following the data.
  PCWSTR HeightKey() const {
    return expanded_ ? L"nativeLastHeightExpanded" : L"nativeLastHeightCollapsed";
  }
  double TargetHeight() const { return gating_ && heldHeight_ > 0 ? heldHeight_ : wantedHeight_; }
  void RememberHeight() {
    if (gating_ || wantedHeight_ <= 0 || (animation_ && animation_.IsEnabled()))
      return;
    int value = int(std::lround(wantedHeight_ * 100));
    int &saved = expanded_ ? savedExpanded_ : savedCollapsed_;
    if (saved != value) {
      saved = value;
      Wh_SetIntValue(HeightKey(), value);
    }
  }
  void StartGate() {
    heldHeight_ = Wh_GetIntValue(HeightKey(), 0) / 100.;
    if (heldHeight_ > 0)
      heldHeight_ = std::min(heldHeight_, PanelCap());
    gating_ = heldHeight_ > 0;
    if (gating_)
      SetPanelHeight(heldHeight_);
    gateJournal_.Save(page_, X::UIElement::OpacityProperty());
    page_.Opacity(0);
    gateStarted_ = GetTickCount64();
    gateStable_ = 0;
    gateLabels_.clear();
    if (!gate_) {
      gate_ = X::DispatcherTimer();
      gate_.Interval(std::chrono::milliseconds(16));
      gateTick_ = gate_.Tick([this](auto &&, auto &&) { GateTick(); });
    }
    gate_.Start();
  }
  void GateTick() {
    if (fading_) {
      auto f = std::min(1., (GetTickCount64() - gateStarted_) / 150.);
      try {
        page_.Opacity(PanelEase(f));
      } catch (...) {
        f = 1;
      }
      if (f >= 1) {
        fading_ = false;
        gate_.Stop();
        gateJournal_.Restore();
      }
      return;
    }
    try {
      if (rowsDirty_) {
        PrepareMixerRows();
        AlignTextBaselines();
        ResizeNative();
      }
      for (auto const &menu : menus_)
        if (menu != input_)
          Summary(menu);
      std::wstring labels;
      for (auto const &menu : menus_)
        labels += std::wstring(menu->label.Text()) + L"|";
      bool ready = DataReady(true);
      gateStable_ = ready && labels == gateLabels_ ? gateStable_ + 1 : ready ? 1 : 0;
      gateLabels_ = labels;
      // Three ticks (~48 ms) with the same labels: provisional selections and new
      // VM row bindings settle before the content is shown.
      if (gateStable_ >= 3 || GetTickCount64() - gateStarted_ >= 400)
        EndGate(ready);
    } catch (...) {
      Wh_Log(L"OpenGateError %08X", winrt::to_hresult());
      EndGate(false);
    }
  }
  void EndGate(bool ready) {
    if (gate_)
      gate_.Stop();
    gating_ = false;
    try {
      for (auto const &menu : menus_)
        if (menu != input_)
          Summary(menu);
      ResizeNative();
      RememberHeight();
    } catch (...) {
    }
    if (winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled() && gate_) {
      // 150 ms fade of the page content with the VM easing; the gate timer drives
      // it, the journal restores the native Opacity when it ends.
      fading_ = true;
      gateStarted_ = GetTickCount64();
      gate_.Start();
    } else
      gateJournal_.Restore();
  }
  // Containers and items of the VM list, in order. The shell keeps adding and
  // reordering sessions while the list first expands; the visual tree callback
  // for new rows arrives frames later, so compare on every layout instead.
  std::wstring RowSignature() {
    std::wstring s;
    auto items = mixerList_.Items();
    auto n = items.Size();
    for (uint32_t i = 0; i < n; i++) {
      s += std::to_wstring(
               reinterpret_cast<uintptr_t>(winrt::get_abi(mixerList_.ContainerFromIndex(i)))) +
           L":" + std::to_wstring(reinterpret_cast<uintptr_t>(winrt::get_abi(items.GetAt(i)))) +
           L";";
    }
    return s;
  }
  std::vector<void *> RowItems() {
    std::vector<void *> v;
    auto items = mixerList_.Items();
    for (uint32_t i = 0; i < items.Size(); i++)
      v.push_back(winrt::get_abi(items.GetAt(i)));
    return v;
  }
  // Spatial sound / output changes make the shell replace the whole session
  // collection in steps (seen: 2 unbound rows at 0 with mute, then 5, then
  // values). Hold the VM height, keep the list transparent until the new rows
  // are named and stable (max 500 ms), then fade/resize with the VM animation.
  // A single app starting or closing keeps most items and is not a rebuild.
  static bool IsRebuild(std::vector<void *> const &before, std::vector<void *> const &now) {
    if (before.empty())
      return false;
    size_t kept = 0;
    for (auto p : before)
      if (std::find(now.begin(), now.end(), p) != now.end())
        kept++;
    return kept * 2 < before.size();
  }
  void StartHold() {
    // A second rebuild can follow within ~100 ms (FxSound restores its output
    // after a selection): stop the crossfade and show the rows as before again.
    if (animation_ && animation_.IsEnabled() && holdAnimation_) {
      animation_.Stop();
      holdAnimation_ = false;
    }
    holding_ = true;
    holdStarted_ = GetTickCount64();
    holdStable_ = 0;
    holdSignature_.clear();
    // LayoutUpdated already reports the first intermediate arrangement; hold the
    // height of the last stable one (seen: the panel grew for the empty phase).
    auto held = std::max(0., stableListHeight_ > 0 ? stableListHeight_ : mixerList_.ActualHeight());
    mixerList_.Height(held);
    // Show the rows as they were (snapshot from the OD/SS flyout opening or the
    // maintenance pass) instead of an empty area; crossfade to the new rows.
    if (crossfade_ && snapshotBrush_ && itemsHost_) {
      itemsHost_.Opacity(0);
      snapshotBrush_.Opacity(1);
      mixerList_.Opacity(1);
    } else {
      M::Imaging::RenderTargetBitmap bitmap{nullptr};
      {
        std::lock_guard guard(snapshot_->lock);
        if (snapshot_->bitmap &&
            (snapshot_->items == rowItems_ || GetTickCount64() - snapshot_->at < 3000) &&
            std::abs(snapshot_->height - held) < 1)
          bitmap = snapshot_->bitmap;
      }
      if (!(bitmap && ShowSnapshot(bitmap)))
        mixerList_.Opacity(0);
    }
    if (!holdTimer_) {
      holdTimer_ = X::DispatcherTimer();
      holdTimer_.Interval(std::chrono::milliseconds(16));
      holdTick_ = holdTimer_.Tick([this](auto &&, auto &&) { HoldTick(); });
    }
    holdTimer_.Start();
  }
  void CaptureMixer() {
    if (!expanded_ || holding_ || !mixerList_ ||
        mixerList_.Visibility() != X::Visibility::Visible ||
        (animation_ && animation_.IsEnabled()) || mixerList_.ActualHeight() < 1)
      return;
    auto shared = snapshot_;
    {
      std::lock_guard guard(shared->lock);
      if (shared->capturing)
        return;
      shared->capturing = true;
    }
    try {
      M::Imaging::RenderTargetBitmap bitmap;
      auto items = rowItems_;
      auto height = mixerList_.ActualHeight();
      bitmap.RenderAsync(mixerList_)
          .Completed([shared, bitmap, items,
                      height](auto const &, winrt::Windows::Foundation::AsyncStatus status) {
            std::lock_guard guard(shared->lock);
            shared->capturing = false;
            if (status == winrt::Windows::Foundation::AsyncStatus::Completed) {
              shared->bitmap = bitmap;
              shared->items = items;
              shared->height = height;
              shared->at = GetTickCount64();
            }
          });
    } catch (...) {
      std::lock_guard guard(shared->lock);
      shared->capturing = false;
      Wh_Log(L"SnapshotError %08X", winrt::to_hresult());
    }
  }
  bool ShowSnapshot(M::Imaging::RenderTargetBitmap const &bitmap) {
    try {
      itemsHost_ = Find(mixerList_,
                        [](auto const &e) { return bool(e.template try_as<C::ItemsPresenter>()); });
      if (!itemsHost_)
        return false;
      savedBackground_ = mixerList_.ReadLocalValue(C::Control::BackgroundProperty());
      snapshotBrush_ = M::ImageBrush();
      snapshotBrush_.ImageSource(bitmap);
      snapshotBrush_.Stretch(M::Stretch::Fill);
      mixerList_.Background(snapshotBrush_);
      itemsHost_.Opacity(0);
      mixerList_.Opacity(1);
      crossfade_ = true;
      return true;
    } catch (...) {
      Wh_Log(L"SnapshotError %08X", winrt::to_hresult());
      ClearSnapshot();
      return false;
    }
  }
  void ClearSnapshot() {
    if (!crossfade_ && !snapshotBrush_)
      return;
    crossfade_ = false;
    try {
      if (itemsHost_)
        itemsHost_.ClearValue(X::UIElement::OpacityProperty());
    } catch (...) {
    }
    try {
      if (!savedBackground_ || savedBackground_ == X::DependencyProperty::UnsetValue())
        mixerList_.ClearValue(C::Control::BackgroundProperty());
      else
        mixerList_.SetValue(C::Control::BackgroundProperty(), savedBackground_);
    } catch (...) {
    }
    snapshotBrush_ = nullptr;
    itemsHost_ = nullptr;
    savedBackground_ = nullptr;
  }
  void HoldTick() {
    try {
      if (rowsDirty_ || RowSignature() != rowSignature_) {
        PrepareMixerRows();
        AlignTextBaselines();
      }
      auto n = mixerList_.Items().Size();
      bool ready = n && !pendingRows_ && mixerList_.ContainerFromIndex(n - 1);
      auto sig = RowSignature();
      holdStable_ = ready && sig == holdSignature_ ? holdStable_ + 1 : ready ? 1 : 0;
      holdSignature_ = sig;
      // The shell adds sessions with pauses of more than 48 ms: ~100 ms of quiet.
      auto waited = GetTickCount64() - holdStarted_;
      // Opening without a refill: the previous list was current; 200 ms of quiet.
      bool settled =
          openHold_ ? (openRebuilt_ ? holdStable_ >= 6 : holdStable_ >= 12) : holdStable_ >= 6;
      if (settled || waited >= 600)
        EndHold();
    } catch (...) {
      Wh_Log(L"RebuildHoldError %08X", winrt::to_hresult());
      EndHold();
    }
  }
  void EndHold() {
    if (!holding_)
      return;
    holding_ = false;
    if (holdTimer_)
      holdTimer_.Stop();
    if (openHold_) {
      openHold_ = false;
      openRebuilt_ = false;
    }
    if (!expanded_ || !winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
      ClearSnapshot();
      mixerList_.Height(NAN);
      mixerList_.Opacity(1);
      ResizeNative();
      return;
    }
    if (auto rotation = arrow_.RenderTransform().try_as<M::RotateTransform>())
      arrowStart_ = rotation.Angle();
    from_ = mixerList_.ActualHeight();
    mixerList_.Height(NAN);
    mixerList_.Measure({float(std::max(1., mixerList_.ActualWidth())), INFINITY});
    to_ = mixerList_.DesiredSize().Height;
    mixerList_.Height(from_);
    start_ = NowMs();
    animFirst_ = true;
    EnsureAnimation();
    holdAnimation_ = true;
    animation_.Start();
    StartAnimationLoop();
  }
  void ScheduleRowsRecheck() {
    if (!rowsTimer_) {
      rowsTimer_ = X::DispatcherTimer();
      rowsTimer_.Interval(std::chrono::milliseconds(16));
      rowsTick_ = rowsTimer_.Tick([this](auto &&, auto &&) {
        rowsTimer_.Stop();
        try {
          if (page_) {
            PrepareMixerRows();
            AlignTextBaselines();
            if (!animation_ || !animation_.IsEnabled())
              ResizeNative();
          }
        } catch (...) {
          Wh_Log(L"RowsRecheckError %08X", winrt::to_hresult());
        }
      });
    }
    if (!rowsTimer_.IsEnabled())
      rowsTimer_.Start();
  }
  void MarkRowsDirty() { rowsDirty_ = true; }
  // Divider -> SV rail and SV rail -> panel edge: one StepPx each. The divider
  // is a 1 DIP border (one pixel), the SV control 32 DIP (centre at 16).
  void FooterMargins() {
    if (!footer_)
      return;
    auto root = page_.XamlRoot();
    double s = root ? root.RasterizationScale() : 1.;
    double step = StepPx(s), half = std::round(16 * s), border = std::max(1., std::round(s));
    footer_.Margin({20, (step - border - half) / s, 20, (step - half) / s});
  }
  // The SV divider lies on the footer's base acrylic, the line above All
  // settings on the tiles' layer (#09FFFFFF, = the Sound content's
  // LayerOnAcrylicFillColorDefault overlay). White over white composes:
  // 1-(1-0.035)(1-0.082) = 0.114 -> #1DFFFFFF gives the same pixel as layer +
  // line on any wallpaper.
  void FooterLine() {
    if (DarkTheme(page_))
      footerBorder_.BorderBrush(DarkBaseDivider());
    else
      PaintDivider(footerBorder_, page_);
  }
  void StartOpenHold() {
    if (!expanded_ || holding_ || !mixerList_ ||
        mixerList_.Visibility() != X::Visibility::Visible ||
        !winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled())
      return;
    openHold_ = true;
    openRebuilt_ = false;
    holding_ = true;
    holdStarted_ = GetTickCount64();
    holdStable_ = 0;
    holdSignature_.clear();
    mixerList_.Opacity(0);
    if (!holdTimer_) {
      holdTimer_ = X::DispatcherTimer();
      holdTimer_.Interval(std::chrono::milliseconds(16));
      holdTick_ = holdTimer_.Tick([this](auto &&, auto &&) { HoldTick(); });
    }
    holdTimer_.Start();
  }
  ULONGLONG userChangeAt_ = 0;
  void BeforeRender() {
    if (!prepared_ || preparing_)
      return;
    // Every slider step re-lays out its template and raises this pass; the VM
    // row checks (signatures over all containers) are skipped while the SV
    // slider is being moved.
    if (GetTickCount64() - userChangeAt_ < 150 && !rowsDirty_ &&
        !(animation_ && animation_.IsEnabled()))
      return;
    // Safety valve: if our layout work repeats within one timer quantum, the
    // layout manager is iterating; stop contributing instead of risking a cycle.
    {
      auto now = GetTickCount64();
      if (now != layoutWorkTick_) {
        layoutWorkTick_ = now;
        layoutWorkCount_ = 0;
      }
      if (++layoutWorkCount_ > 8) {
        return;
      }
    }
    if ((!holding_ || openHold_) && expanded_ &&
        mixerList_.Visibility() == X::Visibility::Visible &&
        (!(animation_ && animation_.IsEnabled()) || holdAnimation_)) {
      auto items = RowItems();
      bool animating = animation_ && animation_.IsEnabled();
      if (items == rowItems_ && !rowsDirty_) {
        if (!animating && !openRebuilt_)
          stableListHeight_ = mixerList_.ActualHeight();
      } else if (IsRebuild(rowItems_, items)) {
        // Opening: the rows are hidden already; keep the list height of the
        // previous rows while the shell refills it, then wait for it to settle.
        if (openHold_) {
          if (!openRebuilt_) {
            openRebuilt_ = true;
            holdStable_ = 0;
            if (stableListHeight_ > 0)
              mixerList_.Height(stableListHeight_);
          }
        } else
          StartHold();
      }
    }
    if (!rowsDirty_ && mixerList_.Visibility() == X::Visibility::Visible &&
        RowSignature() != rowSignature_)
      rowsDirty_ = true;
    if (rowsDirty_ && expanded_ && animation_ && animation_.IsEnabled()) {
      // Rows changed during the expand animation: prepare them and retarget it.
      PrepareMixerRows();
      AlignTextBaselines();
      auto h = mixerList_.Height();
      mixerList_.Height(NAN);
      mixerList_.Measure({float(std::max(1., mixerList_.ActualWidth())), INFINITY});
      to_ = mixerList_.DesiredSize().Height;
      mixerList_.Height(h);
    }
    if (rowsDirty_) {
      PrepareMixerRows();
      AlignTextBaselines();
      ResizeNative();
    }
    // A shell write of the stock L2 height must not survive until the 200ms
    // maintenance pass. Correct it in layout, while this sound page owns it.
    if (auto target = TargetHeight();
        frame_ && target > 0 &&
        (!std::isfinite(frame_.Height()) || std::abs(frame_.Height() - target) > .25)) {
      SetPanelHeight(target);
    }
  }
  // Level column: the number, or the mute glyph at 0.
  static void ShowLevel(AppRow &row, double value) {
    long n = std::lround(value);
    if (row.level)
      row.level.Text(std::to_wstring(n));
    if (row.levelButton) {
      row.levelButton.Visibility(n <= 0 ? X::Visibility::Collapsed : X::Visibility::Visible);
      // At 100 the button is disabled rather than taken out of hit testing: a
      // hit-test change under the pointer leaves the button's pointer-over state
      // set, and its hover plate came back on the next visual state update.
      if (row.levelButton.IsEnabled() != (n < 100))
        row.levelButton.IsEnabled(n < 100);
    }
    if (row.levelMute)
      row.levelMute.Visibility(n <= 0 ? X::Visibility::Visible : X::Visibility::Collapsed);
  }
  void App(C::Slider const &slider, X::Style const &style) {
    if (appIds_.contains(winrt::get_abi(slider)))
      return;
    auto root = Parent(slider).try_as<C::Grid>();
    if (!root)
      return;
    auto toggle = Find(root, [](auto const &e) {
                    return bool(e.template try_as<P::ToggleButton>());
                  }).try_as<P::ToggleButton>();
    if (!toggle)
      return;
    appIds_.insert(winrt::get_abi(slider));
    auto row = std::make_shared<AppRow>();
    row->root = winrt::make_weak(root);
    row->slider = winrt::make_weak(slider);
    row->model = RowModel(slider);
    if (slider.Value() > 0)
      row->last = slider.Value();
    rows_.push_back(row);
    journal_.Save(slider, X::FrameworkElement::StyleProperty());
    slider.Style(style);
    journal_.Save(slider, X::FrameworkElement::MinWidthProperty());
    slider.MinWidth(0);
    journal_.Save(slider, X::FrameworkElement::MarginProperty());
    slider.Margin({12, 0, 0, 0});
    journal_.Save(root, X::FrameworkElement::MarginProperty());
    root.Margin({20, 0, 64, 0});
    Set(root, X::FrameworkElement::HeightProperty(), winrt::box_value(38.));
    SliderGeometry(slider);
    for (auto prop : {X::FrameworkElement::WidthProperty(), X::FrameworkElement::HeightProperty()})
      journal_.Save(toggle, prop);
    toggle.Width(32);
    toggle.Height(32);
    row->toggle = winrt::make_weak(toggle);
    Set(toggle, X::FrameworkElement::MinWidthProperty(), winrt::box_value(0.));
    Set(toggle, X::FrameworkElement::MinHeightProperty(), winrt::box_value(0.));
    Set(toggle, X::FrameworkElement::VerticalAlignmentProperty(),
        winrt::box_value(X::VerticalAlignment::Center));
    Set(toggle, X::FrameworkElement::MarginProperty(), winrt::box_value(X::Thickness{}));
    Set(toggle, C::Control::PaddingProperty(), winrt::box_value(X::Thickness{}));
    Set(toggle, C::Control::HorizontalContentAlignmentProperty(),
        winrt::box_value(X::HorizontalAlignment::Center));
    Set(toggle, C::Control::VerticalContentAlignmentProperty(),
        winrt::box_value(X::VerticalAlignment::Center));
    // The shell badge is anchored to the bottom of the entire 48 DIP row.
    // A button-relative badge instead: right -5, bottom 2 of a 32 DIP icon.
    std::vector<X::FrameworkElement> nativeBadges;
    for (auto const &child : root.Children())
      if (auto e = child.try_as<X::FrameworkElement>();
          e && e != toggle && e != slider && C::Grid::GetColumn(e) == 0)
        nativeBadges.push_back(e);
    for (auto const &badge : nativeBadges)
      Set(badge, X::UIElement::VisibilityProperty(), winrt::box_value(X::Visibility::Collapsed));
    row->badge = C::Border();
    row->badge.Width(13);
    row->badge.Height(13);
    row->badge.CornerRadius({4, 4, 4, 4});
    row->badge.HorizontalAlignment(X::HorizontalAlignment::Right);
    row->badge.VerticalAlignment(X::VerticalAlignment::Center);
    row->badge.Margin({0, 15, -5, 0});
    row->badge.IsHitTestVisible(false);
    row->badge.Child(VolumeGlyph(page_, 0, 11));
    if (auto brush = Resource(page_, L"SolidBackgroundFillColorBaseBrush").try_as<M::Brush>())
      row->badge.Background(brush);
    row->badge.Visibility(slider.Value() <= 0 ? X::Visibility::Visible : X::Visibility::Collapsed);
    C::Canvas::SetZIndex(row->badge, 1);
    // The mute indicator replaces the number in the level column at 0. The
    // app's volume is shown as a number in the column under the SV settings
    // button: 32 DIP wide, 12 DIP after the slider, i.e. inside the row's 64 DIP
    // right margin (negative margin; a Grid does not clip).
    // The number sits in a 32 DIP button with the same hover plate as the mute
    // button: pressing it (1-99) sets the app's volume to 100, written to the
    // session's model like the mouse wheel. At 100 it takes no input; at 0 the
    // mute button takes its place.
    row->levelButton = Button(page_, L"Set volume to 100");
    row->levelButton.HorizontalAlignment(X::HorizontalAlignment::Right);
    row->levelButton.VerticalAlignment(X::VerticalAlignment::Center);
    row->levelButton.Margin({0, 0, -44, 0});
    row->level = C::TextBlock();
    row->level.TextAlignment(X::TextAlignment::Center);
    row->level.HorizontalAlignment(X::HorizontalAlignment::Center);
    row->level.VerticalAlignment(X::VerticalAlignment::Center);
    // 12 DIP (caption size): "100" stays within a glyph's width inside the 32 DIP
    // plate, like the icons beside it. The button style carries a symbol font, so
    // the text font is set explicitly.
    if (pageTitle_)
      row->level.FontFamily(pageTitle_.FontFamily());
    else if (auto font = Resource(page_, L"ContentControlThemeFontFamily").try_as<M::FontFamily>())
      row->level.FontFamily(font);
    row->level.FontSize(12);
    row->level.Text(std::to_wstring(std::lround(slider.Value())));
    row->levelButton.Content(row->level);
    // The text keeps the page title's colour (bound, so it follows the theme)
    // also while the button is disabled at 100.
    if (pageTitle_) {
      D::Binding binding;
      binding.Source(pageTitle_);
      binding.Path(X::PropertyPath(L"Foreground"));
      row->level.SetBinding(C::TextBlock::ForegroundProperty(), binding);
    }
    C::Grid::SetColumnSpan(row->levelButton, std::max(1u, root.ColumnDefinitions().Size()));
    C::Grid::SetRowSpan(row->levelButton, std::max(1u, root.RowDefinitions().Size()));
    Add(root, row->levelButton);
    Keep(row->levelButton.Click(winrt::auto_revoke, [this, row](auto &&, auto &&) {
      try {
        auto s = row->slider.get();
        if (!s)
          return;
        auto value = std::lround(s.Value());
        if (value < 1 || value > 99)
          return;
        if (RowModel(s) != row->model)
          row->model = RowModel(s);
        row->wheelTarget = NAN;
        if (!WriteVolume(s, row->model, 100))
          s.Value(100);
      } catch (...) {
        Wh_Log(L"Mixer level button: %08X", winrt::to_hresult());
      }
    }));
    // At 0 a 32 DIP button with the SV's own glyph (QA_Volume, Mute state, same
    // size) replaces the number; pressing it unmutes like the app icon does: the
    // native toggle is toggled through UI Automation, so its command and the
    // mod's restore run exactly as for a click on the icon.
    row->levelMute = Button(page_, L"Unmute");
    row->levelMute.HorizontalAlignment(X::HorizontalAlignment::Right);
    row->levelMute.VerticalAlignment(X::VerticalAlignment::Center);
    row->levelMute.Margin({0, 0, -44, 0});
    try {
      row->levelIcon = std::make_shared<AnimatedVolume>();
      row->levelIcon->Update(0, true);
      row->levelMute.Content(row->levelIcon->Isolated());
    } catch (...) {
      row->levelIcon.reset();
      row->levelMute.Content(VolumeGlyph(page_, 0, 16));
    }
    {
      auto weakToggle = winrt::make_weak(toggle);
      Keep(row->levelMute.Click(winrt::auto_revoke, [weakToggle](auto &&, auto &&) {
        try {
          if (auto t = weakToggle.get())
            if (auto peer =
                    X::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(t))
              if (auto provider = peer.GetPattern(X::Automation::Peers::PatternInterface::Toggle)
                                      .try_as<X::Automation::Provider::IToggleProvider>())
                provider.Toggle();
        } catch (...) {
          Wh_Log(L"MixerUnmuteError %08X", winrt::to_hresult());
        }
      }));
    }
    C::Grid::SetColumnSpan(row->levelMute, std::max(1u, root.ColumnDefinitions().Size()));
    C::Grid::SetRowSpan(row->levelMute, std::max(1u, root.RowDefinitions().Size()));
    Add(root, row->levelMute);
    ShowLevel(*row, slider.Value());
    // Application images and the native fallback FontIcon remain shell-owned.
    // ButtonBase can mark routed input handled before instance subscribers run.
    // Capture the old value even for handled input, before the native Click.
    auto capture = [this, row] {
      if (auto s = row->slider.get()) {
        if (RowModel(s) != row->model) {
          row->model = RowModel(s);
          row->last = s.Value() > 0 ? s.Value() : 50;
        }
        row->before = s.Value();
        row->beforeValid = true;
      }
    };
    auto pressed =
        winrt::box_value(X::Input::PointerEventHandler([capture](auto &&, auto &&) { capture(); }));
    toggle.AddHandler(X::UIElement::PointerPressedEvent(), pressed, true);
    revoke_.push_back(
        [toggle, pressed] { toggle.RemoveHandler(X::UIElement::PointerPressedEvent(), pressed); });
    auto key = winrt::box_value(
        X::Input::KeyEventHandler([capture](auto &&, X::Input::KeyRoutedEventArgs const &args) {
          using K = winrt::Windows::System::VirtualKey;
          if (args.Key() == K::Space || args.Key() == K::Enter)
            capture();
        }));
    toggle.AddHandler(X::UIElement::PreviewKeyDownEvent(), key, true);
    revoke_.push_back(
        [toggle, key] { toggle.RemoveHandler(X::UIElement::PreviewKeyDownEvent(), key); });
    Keep(toggle.Click(winrt::auto_revoke, [this, row](auto &&, auto &&) {
      try {
        if (auto slider = row->slider.get()) {
          if (RowModel(slider) != row->model) {
            row->model = RowModel(slider);
            row->beforeValid = false;
            row->last = slider.Value() > 0 ? slider.Value() : 50;
          }
          auto before = row->beforeValid ? row->before : slider.Value();
          row->beforeValid = false;
          if (before > 0)
            row->last = before;
          auto next = before > 0 ? 0 : row->last;
          bool sent = WriteVolume(slider, row->model, next);
          row->expected = sent ? next : NAN;
          row->verifyAt = GetTickCount64() + 600;
        }
      } catch (...) {
        Wh_Log(L"Native mute model: %08X", winrt::to_hresult());
      }
    }));
    // Mouse wheel as on SV. Written to the session's model like mute/restore;
    // quick notches add up on the last requested value instead of a slider that
    // has not caught up yet.
    Keep(slider.PointerWheelChanged(
        winrt::auto_revoke, [this, row](auto &&, X::Input::PointerRoutedEventArgs const &args) {
          try {
            auto s = row->slider.get();
            if (!s || args.Handled() || !s.IsEnabled())
              return;
            auto point = args.GetCurrentPoint(s);
            if (point.Properties().IsHorizontalMouseWheel())
              return;
            double delta = point.Properties().MouseWheelDelta() / 120.;
            if (!delta)
              return;
            args.Handled(true);
            if (RowModel(s) != row->model) {
              row->model = RowModel(s);
              row->wheelTarget = NAN;
            }
            auto now = GetTickCount64();
            double base = now - row->wheelAt < 400 && std::isfinite(row->wheelTarget)
                              ? row->wheelTarget
                              : s.Value();
            double step = s.SmallChange() > 0 ? s.SmallChange() : 1;
            double next = std::clamp(std::round(base + delta * step), s.Minimum(), s.Maximum());
            if (next == base)
              return;
            row->wheelTarget = next;
            row->wheelAt = now;
            if (!WriteVolume(s, row->model, next))
              s.Value(next);
          } catch (...) {
            Wh_Log(L"MixerWheelError %08X", winrt::to_hresult());
          }
        }));
    Keep(slider.ValueChanged(winrt::auto_revoke, [row](auto &&, auto const &args) {
      if (args.NewValue() > 0)
        row->last = args.NewValue();
      row->badge.Visibility(args.NewValue() <= 0 ? X::Visibility::Visible
                                                 : X::Visibility::Collapsed);
      ShowLevel(*row, args.NewValue());
    }));
  }
  void Refresh() {
    latest_ = State();
    for (auto const &menu : menus_)
      if (menu != input_)
        Summary(menu);
    auto const &state = latest_;
    ApplyMaster(state);
    SyncInput(state);
    for (auto const &menu : menus_)
      if (menu->open)
        CompactRealized(menu);
    PrepareMixerRows();
    AlignTextBaselines();
    AlignRows();
    ResizeNative();
    RememberHeight();
    {
      bool stale;
      {
        std::lock_guard guard(snapshot_->lock);
        stale = !snapshot_->bitmap || snapshot_->items != rowItems_ ||
                GetTickCount64() - snapshot_->at > 2000;
      }
      if (stale)
        CaptureMixer();
    }
  }
  void Restore() {
    if (restoring_)
      return;
    restoring_ = true;
    if (gate_) {
      gate_.Stop();
      gate_.Tick(gateTick_);
      gate_ = nullptr;
    }
    if (rowsTimer_) {
      rowsTimer_.Stop();
      rowsTimer_.Tick(rowsTick_);
      rowsTimer_ = nullptr;
    }
    if (holdTimer_) {
      holdTimer_.Stop();
      holdTimer_.Tick(holdTick_);
      holdTimer_ = nullptr;
    }
    holding_ = false;
    openHold_ = false;
    ClearSnapshot();
    fading_ = false;
    gateJournal_.Restore();
    audio_.Stop();
    StopAnimationLoop();
    if (animation_) {
      animation_.Stop();
      animation_.Tick(animationTick_);
      animation_ = nullptr;
    }
    if (gearStory_)
      gearStory_.Stop();
    for (auto const &menu : menus_) {
      menu->flyout.Hide();
      if (menu->arrowStory)
        menu->arrowStory.Stop();
    }
    for (auto &r : revoke_)
      try {
        r();
      } catch (...) {
      }
    revoke_.clear();
    journal_.Restore();
    frameJournal_.Restore();
    rows_.clear();
    menus_.clear();
    page_ = nullptr;
  }
  // A page Windows has replaced: stop timers, audio and handlers but leave its
  // controls as they are. Restoring it moves the old Output ListView out of its
  // flyout back into the dead page; its selection is then re-evaluated and the
  // shell can switch the default output, and sound stops.
  void Abandon() {
    if (restoring_)
      return;
    restoring_ = true;
    if (gate_) {
      gate_.Stop();
      gate_.Tick(gateTick_);
      gate_ = nullptr;
    }
    if (rowsTimer_) {
      rowsTimer_.Stop();
      rowsTimer_.Tick(rowsTick_);
      rowsTimer_ = nullptr;
    }
    if (holdTimer_) {
      holdTimer_.Stop();
      holdTimer_.Tick(holdTick_);
      holdTimer_ = nullptr;
    }
    holding_ = false;
    openHold_ = false;
    ClearSnapshot();
    fading_ = false;
    gateJournal_.undo.clear();
    gateJournal_.saved.clear();
    audio_.Stop();
    StopAnimationLoop();
    if (animation_) {
      animation_.Stop();
      animation_.Tick(animationTick_);
      animation_ = nullptr;
    }
    if (gearStory_)
      gearStory_.Stop();
    for (auto const &menu : menus_) {
      menu->flyout.Hide();
      if (menu->arrowStory)
        menu->arrowStory.Stop();
    }
    for (auto &r : revoke_)
      try {
        r();
      } catch (...) {
      }
    revoke_.clear();
    journal_.undo.clear();
    journal_.saved.clear();
    frameJournal_.Restore();
    rows_.clear();
    menus_.clear();
    page_ = nullptr;
  }
  ~SoundPage() { Restore(); }
};
} // namespace native

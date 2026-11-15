#pragma once
#include "layout.h"
#include "quick.h"
// Public visual-tree discovery from ControlCenter.dll COM-interface hooks.
// No XAML Diagnostics connection, TAP, or diagnostic instance handles.
namespace native {
inline HWND ThreadWindow(DWORD id) {
  std::vector<HWND> found;
  EnumThreadWindows(
      id,
      [](HWND w, LPARAM p) -> BOOL {
        WCHAR name[96]{};
        GetClassNameW(w, name, 96);
        if (!_wcsicmp(name, L"ControlCenterWindow") ||
            !_wcsicmp(name, L"Windows.UI.Core.CoreWindow"))
          reinterpret_cast<std::vector<HWND> *>(p)->push_back(w);
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&found));
  for (auto handle : found) {
    WCHAR name[96]{};
    GetClassNameW(handle, name, ARRAYSIZE(name));
    if (!_wcsicmp(name, L"ControlCenterWindow"))
      return handle;
  }
  return found.empty() ? nullptr : found.front();
}
inline UINT dispatchMessage = RegisterWindowMessageW(L"Windhawk.SoundNative.Dispatch.2");
struct Job {
  std::function<void()> action;
  bool succeeded = false;
};
inline std::mutex jobsMutex;
inline std::map<UINT_PTR, Job *> jobs;
inline std::atomic<UINT_PTR> nextJob = 0;
inline LRESULT CALLBACK DispatchHook(int code, WPARAM w, LPARAM l) {
  if (code >= 0) {
    auto msg = reinterpret_cast<CWPSTRUCT *>(l);
    if (msg->message == dispatchMessage) {
      Job *job = nullptr;
      {
        std::lock_guard lock(jobsMutex);
        auto i = jobs.find(msg->wParam);
        if (i != jobs.end())
          job = i->second;
      }
      if (job)
        try {
          job->action();
          job->succeeded = true;
        } catch (...) {
          Wh_Log(L"UI dispatch: %08X", winrt::to_hresult());
        }
    }
  }
  return CallNextHookEx(nullptr, code, w, l);
}
inline bool Dispatch(HWND window, std::function<void()> action) {
  auto thread = GetWindowThreadProcessId(window, nullptr);
  if (!thread)
    return false;
  if (thread == GetCurrentThreadId()) {
    try {
      action();
      return true;
    } catch (...) {
      Wh_Log(L"UI dispatch: %08X", winrt::to_hresult());
      return false;
    }
  }
  auto hook = SetWindowsHookExW(WH_CALLWNDPROC, DispatchHook, module, thread);
  if (!hook)
    return false;
  Job job{std::move(action)};
  auto cookie = ++nextJob;
  {
    std::lock_guard lock(jobsMutex);
    jobs[cookie] = &job;
  }
  SendMessageW(window, dispatchMessage, cookie, 0);
  {
    std::lock_guard lock(jobsMutex);
    jobs.erase(cookie);
  }
  UnhookWindowsHookEx(hook);
  return job.succeeded;
}

struct Runtime;
inline void CALLBACK PanelVisibilityChanged(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
inline std::mutex runtimesMutex;
inline std::map<DWORD, Runtime *> runtimes;
struct Runtime {
  HWND window = nullptr, pulseWindow = nullptr;
  HWND presentedSound = nullptr;
  winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
  HWINEVENTHOOK showHook = nullptr, uncloakHook = nullptr, hideHook = nullptr, cloakHook = nullptr;
  X::DispatcherTimer timer{nullptr}, discoveryTimer{nullptr};
  winrt::event_token tick{}, discoveryTick{};
  std::vector<winrt::weak_ref<X::FrameworkElement>> discoveryCandidates;
  unsigned discoveryAttempts = 0;
  bool busy = false, discovering = false;
  winrt::weak_ref<X::FrameworkElement> l2frame;
  int64_t frameToken = 0, frameVisibilityToken = 0;
  std::vector<std::unique_ptr<SoundPage>> pages;
  std::vector<std::unique_ptr<QuickPage>> quickPages;
  struct PageWatch {
    winrt::weak_ref<X::FrameworkElement> page;
    winrt::event_token loading{}, loaded{}, layout{};
    std::unique_ptr<Journal> firstFrame;
    ULONGLONG hiddenAt = 0;
  };
  std::vector<PageWatch> pageWatches;
  struct ViewWatch {
    winrt::weak_ref<X::FrameworkElement> view;
    winrt::event_token loaded{}, layout{};
  };
  std::vector<ViewWatch> viewWatches;
  std::shared_ptr<VolumePulse> volumePulse;
  ModSettings appliedSettings = SettingsSnapshot();
  bool volumeDuringRefresh = false;
  bool Hidden() const { return !WindowShown(window); }
  void ClearSoundPresentation() {
    if (presentedSound)
      RemovePropW(presentedSound, soundPageProperty);
    presentedSound = nullptr;
  }
  void ReportSoundPresentation(DWORD shownAt = 0) {
    auto frame = l2frame.get().try_as<C::ContentControl>();
    auto page = frame ? frame.Content().try_as<X::FrameworkElement>() : nullptr;
    bool sound = page && page.IsLoaded() && page.XamlRoot() &&
                 Class(page).find(L"ControlCenter.FullScreenPage") != std::wstring::npos &&
                 (page.FindName(L"OutputGroupTitle") || Named(page, L"OutputGroupTitle"));
    if (sound)
      for (auto element = page; element; element = Parent(element))
        if (element.Visibility() != X::Visibility::Visible ||
            (element != page && element.Opacity() <= 0)) {
          sound = false;
          break;
        }
    auto monitor = sound && window ? MonitorFromWindow(window, MONITOR_DEFAULTTONULL) : nullptr;
    bool shown = monitor && !Hidden();
    if (presentedSound && (!shown || presentedSound != window))
      ClearSoundPresentation();
    if (shown && (GetPropW(window, soundPageProperty) == monitor ||
                  SetPropW(window, soundPageProperty, monitor)))
      presentedSound = window;
    if (!monitor || (!shown && !shownAt))
      return;
    // WinEvent callbacks can be queued behind a close. A show event for the
    // current Sound page is still evidence that the request activated it.
    for (auto type : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"})
      for (HWND taskbar = nullptr; (taskbar = FindWindowExW(nullptr, taskbar, type, nullptr));)
        if (auto token = reinterpret_cast<UINT_PTR>(GetPropW(taskbar, soundRequestProperty));
            token && MonitorFromWindow(taskbar, MONITOR_DEFAULTTONULL) == monitor &&
            (!shownAt || SoundEventMatchesRequest(token, shownAt)))
          PostMessageW(taskbar, soundShownMessage, token, reinterpret_cast<LPARAM>(monitor));
  }
  void Wake() {
    if (timer && !stopping) {
      timer.Interval(std::chrono::milliseconds(200));
      timer.Start();
    }
  }
  static LRESULT CALLBACK VolumeProc(HWND handle, UINT message, WPARAM w, LPARAM l,
                                     DWORD_PTR reference) {
    auto self = reinterpret_cast<Runtime *>(reference);
    if (message == volumePulseMessage) {
      if (self->volumePulse)
        self->volumePulse->queued = false;
      if (!stopping)
        try {
          if (self->busy)
            self->volumeDuringRefresh = true;
          else
            for (auto const &page : self->pages)
              page->RefreshMasterNow();
        } catch (...) {
          Wh_Log(L"MasterPulseError %08X", winrt::to_hresult());
        }
      return 0;
    }
    if (message == WM_NCDESTROY) {
      if (self->presentedSound == handle)
        self->ClearSoundPresentation();
      if (self->pulseWindow == handle) {
        if (self->volumePulse)
          self->volumePulse->active = false;
        self->pulseWindow = nullptr;
      }
      if (self->window == handle)
        self->window = nullptr;
      WindhawkUtils::RemoveWindowSubclassFromAnyThread(handle, VolumeProc);
    }
    return DefSubclassProc(handle, message, w, l);
  }
  void EnsureVolumePulse() {
    HWND next = window && IsWindow(window) ? window : ThreadWindow(GetCurrentThreadId());
    if (!next || GetWindowThreadProcessId(next, nullptr) != GetCurrentThreadId())
      return;
    window = next;
    if (pulseWindow == next && IsWindow(pulseWindow))
      return;
    if (volumePulse)
      volumePulse->active = false;
    if (pulseWindow && IsWindow(pulseWindow))
      WindhawkUtils::RemoveWindowSubclassFromAnyThread(pulseWindow, VolumeProc);
    pulseWindow = nullptr;
    auto fresh = std::make_shared<VolumePulse>(next);
    if (!WindhawkUtils::SetWindowSubclassFromAnyThread(next, VolumeProc,
                                                       reinterpret_cast<DWORD_PTR>(this))) {
      fresh->active = false;
      Wh_Log(L"Could not subclass the Control Center window");
      return;
    }
    pulseWindow = next;
    volumePulse = std::move(fresh);
  }
  explicit Runtime(X::FrameworkElement const &anchor)
      : window(ThreadWindow(GetCurrentThreadId())), dispatcher(anchor.Dispatcher()) {
    timer = X::DispatcherTimer();
    timer.Interval(std::chrono::milliseconds(200));
    tick = timer.Tick([this](auto &&, auto &&) { Refresh(); });
    discoveryTimer = X::DispatcherTimer();
    discoveryTimer.Interval(std::chrono::milliseconds(1));
    discoveryTick = discoveryTimer.Tick([this](auto &&, auto &&) { DiscoverPending(); });
    DWORD process = GetCurrentProcessId(), thread = GetCurrentThreadId();
    auto watch = [process, thread](DWORD event) {
      return SetWinEventHook(event, event, nullptr, PanelVisibilityChanged, process, thread,
                             WINEVENT_OUTOFCONTEXT);
    };
    showHook = watch(EVENT_OBJECT_SHOW);
    uncloakHook = watch(EVENT_OBJECT_UNCLOAKED);
    hideHook = watch(EVENT_OBJECT_HIDE);
    cloakHook = watch(EVENT_OBJECT_CLOAKED);
    std::lock_guard lock(runtimesMutex);
    runtimes[thread] = this;
  }
  void OnFrameContent(X::DependencyObject const &sender) {
    auto frame = sender.try_as<C::ContentControl>();
    if (!frame)
      return;
    auto page = frame.Content().try_as<X::FrameworkElement>();
    ReportSoundPresentation();
    if (!page || Class(page).find(L"ControlCenter.FullScreenPage") == std::wstring::npos)
      return;
    WatchPage(page);
    Wake();
    BeforeDisplay(page, true);
  }
  void WatchFrame(X::FrameworkElement const &frame) {
    if (auto current = l2frame.get(); current && current == frame)
      return;
    UnwatchFrame();
    if (!frame.try_as<C::ContentControl>())
      return;
    l2frame = winrt::make_weak(frame);
    frameToken = frame.RegisterPropertyChangedCallback(
        C::ContentControl::ContentProperty(),
        [this](X::DependencyObject const &sender, X::DependencyProperty const &) {
          try {
            OnFrameContent(sender);
          } catch (...) {
          }
        });
    frameVisibilityToken = frame.RegisterPropertyChangedCallback(
        X::UIElement::VisibilityProperty(),
        [this](X::DependencyObject const &, X::DependencyProperty const &) {
          try {
            ReportSoundPresentation();
          } catch (...) {
            Wh_Log(L"Sound page visibility: %08X", winrt::to_hresult());
          }
        });
    OnFrameContent(frame);
  }
  void UnwatchFrame() {
    ClearSoundPresentation();
    if (auto frame = l2frame.get())
      for (auto const &[property, token] :
           {std::pair{C::ContentControl::ContentProperty(), frameToken},
            std::pair{X::UIElement::VisibilityProperty(), frameVisibilityToken}})
        if (token)
          try {
            frame.UnregisterPropertyChangedCallback(property, token);
          } catch (...) {
          }
    l2frame = nullptr;
    frameToken = frameVisibilityToken = 0;
  }
  bool IsCurrentPage(X::FrameworkElement const &page) const {
    auto frame = l2frame.get().try_as<C::ContentControl>();
    return !frame || frame.Content() == page;
  }
  void PreparePage(X::FrameworkElement const &page, bool opening) {
    if (Retired(page) || !IsCurrentPage(page))
      return;
    for (auto const &item : pages)
      if (item->Page() == page) {
        if (opening)
          item->PrepareInitialLayout();
        else
          item->BeforeRender();
        return;
      }
    if ((!page.IsLoaded() && !Parent(page)) || !Named(page, L"OutputGroupTitle"))
      return;
    RetireOthers(page);
    auto pulse = volumePulse;
    auto patch = std::make_unique<SoundPage>(page, window, [pulse] {
      if (pulse)
        pulse->Post();
    });
    if (patch->Apply()) {
      if (auto watch = std::find_if(pageWatches.begin(), pageWatches.end(),
                                   [&](auto const &item) { return item.page.get() == page; });
          watch != pageWatches.end())
        watch->firstFrame.reset();
      patch->PrepareInitialLayout();
      patch->BeforeRender();
      pages.push_back(std::move(patch));
      Wh_Log(L"Sound page attached through ControlCenter symbols");
    }
  }
  void BeforeDisplay(X::FrameworkElement const &page, bool opening) noexcept {
    if (stopping || busy)
      return;
    busy = true;
    try {
      EnsureVolumePulse();
      ReportSoundPresentation();
      PreparePage(page, opening);
    } catch (...) {
      Wh_Log(L"Initial native layout: %08X", winrt::to_hresult());
    }
    busy = false;
    if (volumeDuringRefresh) {
      volumeDuringRefresh = false;
      if (volumePulse)
        volumePulse->Post();
    }
  }
  // The shell can retain old page objects. Only a new current Sound page
  // retires them and stops their handlers.
  std::vector<winrt::weak_ref<X::FrameworkElement>> retired;
  bool Retired(X::FrameworkElement const &page) {
    for (auto it = retired.begin(); it != retired.end();) {
      auto p = it->get();
      if (!p) {
        it = retired.erase(it);
        continue;
      }
      if (p == page)
        return true;
      ++it;
    }
    return false;
  }
  void RetireOthers(X::FrameworkElement const &page) {
    for (auto it = pageWatches.begin(); it != pageWatches.end();) {
      auto existing = it->page.get();
      if (existing && existing == page) {
        ++it;
        continue;
      }
      if (existing) {
        existing.Loading(it->loading);
        existing.Loaded(it->loaded);
        existing.LayoutUpdated(it->layout);
        retired.push_back(it->page);
      }
      it = pageWatches.erase(it);
    }
    for (auto it = pages.begin(); it != pages.end();) {
      if ((*it)->Page() == page) {
        ++it;
        continue;
      }
      if (auto old = (*it)->Page())
        retired.push_back(winrt::make_weak(old));
      (*it)->Abandon();
      it = pages.erase(it);
    }
  }
  // Same for Quick Settings, in case the shell also rebuilds ControlCenterView.
  winrt::weak_ref<X::FrameworkElement> newestQuick;
  void RetireQuick() {
    auto view = newestQuick.get();
    if (!view)
      return;
    newestQuick = nullptr;
    for (auto it = quickPages.begin(); it != quickPages.end();) {
      if ((*it)->View() == view) {
        ++it;
        continue;
      }
      (*it)->Restore();
      it = quickPages.erase(it);
    }
  }
  void WatchPage(X::FrameworkElement const &page) {
    // Discovery also visits pages that the shell is building off-screen.
    if (Retired(page) || !IsCurrentPage(page))
      return;
    for (auto it = pageWatches.begin(); it != pageWatches.end();)
      if (auto existing = it->page.get()) {
        if (existing == page)
          return;
        ++it;
      } else
        it = pageWatches.erase(it);
    auto weak = winrt::make_weak(page);
    PageWatch watch;
    watch.page = weak;
    // Content can be assigned before Loaded, and its stock layout can be
    // rendered during the native entrance. Hold that content until Apply()
    // has rebuilt it. The native surface and its animation remain active.
    if (page.FindName(L"OutputGroupTitle") || Named(page, L"OutputGroupTitle")) {
      watch.firstFrame = std::make_unique<Journal>();
      watch.firstFrame->Save(page, X::UIElement::OpacityProperty());
      page.Opacity(0);
      watch.hiddenAt = GetTickCount64();
    }
    watch.loading = page.Loading([this, weak](auto &&, auto &&) {
      if (auto page = weak.get())
        BeforeDisplay(page, true);
    });
    watch.loaded = page.Loaded([this, weak](auto &&, auto &&) {
      if (auto page = weak.get())
        BeforeDisplay(page, true);
    });
    watch.layout = page.LayoutUpdated([this, weak](auto &&, auto &&) {
      if (auto page = weak.get())
        BeforeDisplay(page, false);
    });
    pageWatches.push_back(std::move(watch));
  }

  void WatchView(X::FrameworkElement const &view) {
    for (auto it = viewWatches.begin(); it != viewWatches.end();) {
      if (auto existing = it->view.get()) {
        if (existing == view)
          return;
        ++it;
      } else
        it = viewWatches.erase(it);
    }
    auto weak = winrt::make_weak(view);
    auto retry = [this, weak](auto &&, auto &&) {
      if (stopping || discovering || l2frame.get())
        return;
      if (auto view = weak.get())
        Discover(view);
    };
    ViewWatch watch{weak};
    watch.loaded = view.Loaded(retry);
    watch.layout = view.LayoutUpdated(retry);
    viewWatches.push_back(std::move(watch));
  }
  void Track(X::FrameworkElement const &element) {
    auto type = Class(element);
    if (type == L"ControlCenter.ControlCenterView") {
      WatchView(element);
      bool known = false;
      for (auto const &q : quickPages)
        if (q->View() == element) {
          q->Dirty();
          known = true;
          break;
        }
      if (!known) {
        quickPages.push_back(std::make_unique<QuickPage>(element, window));
        newestQuick = winrt::make_weak(element);
        Wh_Log(L"Control Center view discovered through symbols");
      }
    }
    if (element.Name() == L"L2Frame")
      WatchFrame(element);
    if (type.find(L"ControlCenter.FullScreenPage") != std::wstring::npos) {
      WatchPage(element);
      if (element.IsLoaded())
        BeforeDisplay(element, false);
    }
  }
  bool Discover(X::FrameworkElement const &element) {
    if (!element || stopping || discovering)
      return false;
    discovering = true;
    bool found = false;
    try {
      auto root = element;
      if (auto xaml = element.XamlRoot()) {
        if (auto content = xaml.Content().try_as<X::FrameworkElement>())
          root = content;
      } else {
        for (int depth = 0; depth < 64; ++depth) {
          auto parent = Parent(root);
          if (!parent)
            break;
          root = parent;
        }
      }
      std::vector<X::FrameworkElement> targets;
      Find(root, [&](auto const &item) {
        auto type = Class(item);
        if (type == L"ControlCenter.ControlCenterView" || item.Name() == L"L2Frame" ||
            type.find(L"ControlCenter.FullScreenPage") != std::wstring::npos)
          targets.push_back(item);
        return false;
      });
      // Tree traversal finishes before patching anything in that tree.
      EnsureVolumePulse();
      for (auto const &target : targets) {
        Track(target);
        found = true;
      }
      Wake();
      Refresh();
    } catch (...) {
      Wh_Log(L"Control Center discovery: %08X", winrt::to_hresult());
    }
    discovering = false;
    return found;
  }
  void QueueDiscovery(X::FrameworkElement const &child) {
    if (stopping)
      return;
    // Connect runs inside InitializeComponent: keep only a weak reference to
    // its completed child, and never query or retain the half-constructed view.
    if (discoveryCandidates.size() < 64)
      discoveryCandidates.push_back(winrt::make_weak(child));
    if (!discoveryAttempts) {
      discoveryAttempts = 1;
      discoveryTimer.Interval(std::chrono::milliseconds(1));
      discoveryTimer.Start();
    }
  }
  void DiscoverPending() {
    discoveryTimer.Stop();
    if (stopping) {
      discoveryCandidates.clear();
      discoveryAttempts = 0;
      return;
    }
    auto candidates = std::move(discoveryCandidates);
    discoveryCandidates.clear();
    bool found = false;
    for (auto const &weak : candidates)
      if (auto child = weak.get())
        if (Discover(child)) {
          found = true;
          break;
        }
    if (found || discoveryAttempts++ >= 8) {
      discoveryCandidates.clear();
      discoveryAttempts = 0;
    } else {
      for (auto &weak : candidates)
        if (discoveryCandidates.size() < 64)
          discoveryCandidates.push_back(std::move(weak));
      discoveryTimer.Interval(std::chrono::milliseconds(16));
      discoveryTimer.Start();
    }
  }
  void Refresh() {
    if (stopping || busy)
      return;
    busy = true;
    try {
      EnsureVolumePulse();
      ReportSoundPresentation();
      RetireQuick();
      ApplySettings();
      for (auto &watch : pageWatches)
        if (watch.firstFrame && GetTickCount64() - watch.hiddenAt >= 400)
          watch.firstFrame.reset();
      // Loading and layout events can temporarily detach the page or its
      // output heading while the frame still owns it. Preserve its controls
      // and handlers. RetireOthers handles replacement of the frame content.
      for (auto const &page : pages)
        if (page->Loaded())
          page->Refresh();
      for (auto const &q : quickPages)
        q->Refresh();
    } catch (...) {
      Wh_Log(L"Native layout: %08X", winrt::to_hresult());
    }
    if (timer) {
      if (Hidden() && showHook && uncloakHook)
        timer.Stop();
      else {
        auto interval = std::chrono::milliseconds(Hidden() ? 2000 : 200);
        if (timer.Interval() != winrt::Windows::Foundation::TimeSpan(interval))
          timer.Interval(interval);
      }
    }
    busy = false;
    if (volumeDuringRefresh) {
      volumeDuringRefresh = false;
      if (volumePulse)
        volumePulse->Post();
    }
  }
  void ApplySettings() {
    auto next = SettingsSnapshot();
    if (appliedSettings == next)
      return;
    for (auto const &page : pages)
      page->ApplySettings(next);
    for (auto &quick : quickPages)
      if (!quick->ApplySettings(next)) {
        auto view = quick->View();
        quick->Restore();
        quick = std::make_unique<QuickPage>(view, window);
      }
    appliedSettings = std::move(next);
    Wh_Log(L"Applied settings to %zu Quick Settings views and %zu Sound pages",
           quickPages.size(), pages.size());
  }
  void Stop() {
    if (timer) {
      timer.Stop();
      timer.Tick(tick);
    }
    if (discoveryTimer) {
      discoveryTimer.Stop();
      discoveryTimer.Tick(discoveryTick);
    }
    discoveryCandidates.clear();
    for (auto hook : {showHook, uncloakHook, hideHook, cloakHook})
      if (hook)
        UnhookWinEvent(hook);
    showHook = uncloakHook = hideHook = cloakHook = nullptr;
    if (volumePulse)
      volumePulse->active = false;
    if (pulseWindow && IsWindow(pulseWindow))
      WindhawkUtils::RemoveWindowSubclassFromAnyThread(pulseWindow, VolumeProc);
    pulseWindow = nullptr;
    UnwatchFrame();
    for (auto const &watch : viewWatches)
      if (auto view = watch.view.get()) {
        view.Loaded(watch.loaded);
        view.LayoutUpdated(watch.layout);
      }
    viewWatches.clear();
    for (auto const &watch : pageWatches)
      if (auto page = watch.page.get()) {
        page.Loading(watch.loading);
        page.Loaded(watch.loaded);
        page.LayoutUpdated(watch.layout);
      }
    pageWatches.clear();
    for (auto &q : quickPages)
      q->Restore();
    quickPages.clear();
    for (auto &page : pages)
      page->Restore();
    pages.clear();
    modelMembers.clear();
    timer = nullptr;
    discoveryTimer = nullptr;
    std::lock_guard lock(runtimesMutex);
    if (auto it = runtimes.find(GetCurrentThreadId()); it != runtimes.end() && it->second == this)
      runtimes.erase(it);
  }
};
// UI-thread state is explicitly stopped on unload; no XAML destruction during
// process/thread termination after the other threads have already disappeared.
[[clang::no_destroy]] inline thread_local std::unique_ptr<Runtime> runtime;
inline void CALLBACK PanelVisibilityChanged(HWINEVENTHOOK, DWORD event, HWND handle, LONG object,
                                            LONG child, DWORD, DWORD shownAt) {
  if (stopping || object != OBJID_WINDOW || child != CHILDID_SELF || !runtime || !handle)
    return;
  WCHAR name[96]{};
  GetClassNameW(handle, name, ARRAYSIZE(name));
  if (_wcsicmp(name, L"ControlCenterWindow"))
    return;
  if (event == EVENT_OBJECT_HIDE || event == EVENT_OBJECT_CLOAKED) {
    if (runtime->presentedSound == handle)
      runtime->ClearSoundPresentation();
    return;
  }
  // This is only the visibility handle. EnsureVolumePulse owns pulseWindow.
  runtime->window = handle;
  try {
    runtime->ReportSoundPresentation(shownAt);
  } catch (...) {
    Wh_Log(L"Sound page presentation: %08X", winrt::to_hresult());
  }
  runtime->Wake();
}
inline void RefreshHostSettings() {
  std::vector<HWND> windows;
  {
    std::lock_guard lock(runtimesMutex);
    for (auto const &[thread, state] : runtimes)
      if (auto handle = ThreadWindow(thread))
        windows.push_back(handle);
  }
  for (auto window : windows)
    Dispatch(window, [] {
      if (runtime)
        runtime->Wake();
    });
}
inline void StopBridge() {
  std::vector<std::pair<DWORD, Runtime *>> copy;
  {
    std::lock_guard lock(runtimesMutex);
    for (auto pair : runtimes)
      copy.push_back(pair);
  }
  auto stop = [] {
    if (runtime) {
      runtime->Stop();
      runtime.reset();
    }
  };
  for (auto [thread, state] : copy) {
    auto threadHandle = OpenThread(SYNCHRONIZE, FALSE, thread);
    bool exited = threadHandle && WaitForSingleObject(threadHandle, 0) == WAIT_OBJECT_0;
    if (threadHandle)
      CloseHandle(threadHandle);
    if (exited) {
      // Its UI callbacks cannot run; do not release XAML references off-thread.
      Wh_Log(L"Control Center UI thread %u has exited; retaining its XAML state", thread);
      continue;
    }
    HWND handle = state->pulseWindow && IsWindow(state->pulseWindow) ? state->pulseWindow
                                                                     : ThreadWindow(thread);
    if (handle && GetWindowThreadProcessId(handle, nullptr) == thread && Dispatch(handle, stop))
      continue;
    // A dispatcher shutting down can complete without invoking the delegate.
    // Keep unreachable thread-affine state, but never pin this mod's DLL.
    auto dispatcher = state->dispatcher;
    if (!DispatchCleanup(dispatcher, stop))
      Wh_Log(L"Control Center UI thread %u is shutting down; retaining its XAML state", thread);
  }
}
} // namespace native

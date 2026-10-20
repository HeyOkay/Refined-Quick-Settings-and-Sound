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
inline void CALLBACK PanelShown(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
inline std::mutex runtimesMutex;
inline std::map<DWORD, Runtime *> runtimes;
struct Runtime {
  HWND window = nullptr, pulseWindow = nullptr;
  winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
  HWINEVENTHOOK showHook = nullptr, uncloakHook = nullptr;
  X::DispatcherTimer timer{nullptr}, discoveryTimer{nullptr};
  winrt::event_token tick{}, discoveryTick{};
  std::vector<winrt::weak_ref<X::FrameworkElement>> discoveryCandidates;
  unsigned discoveryAttempts = 0;
  bool busy = false, discovering = false;
  winrt::weak_ref<X::FrameworkElement> l2frame;
  int64_t frameToken = 0;
  std::vector<std::unique_ptr<SoundPage>> pages;
  std::vector<std::unique_ptr<QuickPage>> quickPages;
  struct PageWatch {
    winrt::weak_ref<X::FrameworkElement> page;
    winrt::event_token loaded{}, layout{};
  };
  std::vector<PageWatch> pageWatches;
  struct ViewWatch {
    winrt::weak_ref<X::FrameworkElement> view;
    winrt::event_token loaded{}, layout{};
  };
  std::vector<ViewWatch> viewWatches;
  std::shared_ptr<VolumePulse> volumePulse;
  bool volumeDuringRefresh = false;
  bool Hidden() const { return !WindowShown(window); }
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
    showHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, PanelShown, process,
                               thread, WINEVENT_OUTOFCONTEXT);
    uncloakHook = SetWinEventHook(0x8018 /*EVENT_OBJECT_UNCLOAKED*/, 0x8018, nullptr, PanelShown,
                                  process, thread, WINEVENT_OUTOFCONTEXT);
    std::lock_guard lock(runtimesMutex);
    runtimes[thread] = this;
  }
  void OnFrameContent(X::DependencyObject const &sender) {
    auto frame = sender.try_as<C::ContentControl>();
    if (!frame)
      return;
    auto page = frame.Content().try_as<X::FrameworkElement>();
    if (!page || Class(page).find(L"ControlCenter.FullScreenPage") == std::wstring::npos)
      return;
    WatchPage(page);
    Wake();
    if (page.IsLoaded())
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
    OnFrameContent(frame);
  }
  void UnwatchFrame() {
    if (auto frame = l2frame.get(); frame && frameToken)
      try {
        frame.UnregisterPropertyChangedCallback(C::ContentControl::ContentProperty(), frameToken);
      } catch (...) {
      }
    l2frame = nullptr;
    frameToken = 0;
  }
  void PreparePage(X::FrameworkElement const &page, bool opening) {
    if (!page.IsLoaded() || Retired(page))
      return;
    for (auto const &item : pages)
      if (item->Page() == page) {
        if (opening)
          item->PrepareInitialLayout();
        else
          item->BeforeRender();
        return;
      }
    if (!Named(page, L"OutputGroupTitle"))
      return;
    auto pulse = volumePulse;
    auto patch = std::make_unique<SoundPage>(page, window, ModelAccess{}, [pulse] {
      if (pulse)
        pulse->Post();
    });
    if (patch->Apply()) {
      patch->PrepareInitialLayout();
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
      RetirePending();
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
  // Windows builds a new Sound FullScreenPage for every opening and the old one
  // stays loaded. If every old page kept its patch, timers and layout handlers,
  // each opening would get slower. A newly created page retires all others:
  // patch restored, handlers removed, never patched again.
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
  winrt::weak_ref<X::FrameworkElement> newestPage;
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
  void RetirePending() {
    if (auto page = newestPage.get()) {
      newestPage = nullptr;
      RetireOthers(page);
    }
  }
  void WatchPage(X::FrameworkElement const &page) {
    if (Retired(page))
      return;
    for (auto it = pageWatches.begin(); it != pageWatches.end();)
      if (auto existing = it->page.get()) {
        if (existing == page)
          return;
        ++it;
      } else
        it = pageWatches.erase(it);
    // Retirement is deferred until BeforeDisplay or Refresh. BeforeDisplay
    // (Loaded/LayoutUpdated of the new page) and Refresh retire the others.
    newestPage = winrt::make_weak(page);
    auto weak = winrt::make_weak(page);
    PageWatch watch;
    watch.page = weak;
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
      RetirePending();
      RetireQuick();
      for (auto it = pages.begin(); it != pages.end();)
        if (!(*it)->Alive()) {
          (*it)->Abandon();
          it = pages.erase(it);
        } else {
          (*it)->Refresh();
          ++it;
        }
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
    for (auto hook : {showHook, uncloakHook})
      if (hook)
        UnhookWinEvent(hook);
    showHook = uncloakHook = nullptr;
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
inline void CALLBACK PanelShown(HWINEVENTHOOK, DWORD, HWND handle, LONG object, LONG, DWORD,
                                DWORD) {
  if (stopping || object != OBJID_WINDOW || !runtime || !handle)
    return;
  WCHAR name[96]{};
  GetClassNameW(handle, name, ARRAYSIZE(name));
  if (_wcsicmp(name, L"ControlCenterWindow"))
    return;
  // This is only the visibility handle. EnsureVolumePulse owns pulseWindow.
  runtime->window = handle;
  runtime->Wake();
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
    auto cleaned = std::make_shared<std::atomic<bool>>(false);
    bool stopped = false;
    try {
      // Keep a local dispatcher reference: stop() destroys the Runtime.
      if (auto dispatcher = state->dispatcher) {
        auto operation = dispatcher.RunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::High, [stop, cleaned] {
              try {
                stop();
                *cleaned = true;
              } catch (...) {
                Wh_Log(L"Control Center shutdown: %08X", winrt::to_hresult());
              }
            });
        // AsyncStatus becomes Completed after the UI delegate returns. An
        // event signaled inside the delegate could wake unload too early.
        // This short wait exists only during unload, never in the background.
        while (operation.Status() == winrt::Windows::Foundation::AsyncStatus::Started)
          Sleep(1);
        stopped = operation.Status() == winrt::Windows::Foundation::AsyncStatus::Completed &&
                  cleaned->load();
      }
    } catch (...) {
      Wh_Log(L"Control Center shutdown dispatch: %08X", winrt::to_hresult());
    }
    if (!stopped) {
      // Wh_ModUninit cannot refuse unload. In this exceptional failure case,
      // retain a module reference rather than leave live callbacks unmapped.
      HMODULE retained = nullptr;
      GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                         reinterpret_cast<LPCWSTR>(&StopBridge), &retained);
      Wh_Log(L"Could not stop UI thread %u; keeping module loaded for its callbacks", thread);
    }
  }
}
} // namespace native

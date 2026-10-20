#pragma once
#include "layout.h"
#include "quick.h"
// XAML Diagnostics TAP integration, independently implemented using the SDK.
// Lifecycle/connection approach: m417z Notification Center Styler (GPL-3.0).
namespace native {
inline constexpr GUID tapClsid = {
    0x83110e61, 0x2ad1, 0x41a2, {0x94, 0xae, 0xba, 0x5a, 0x22, 0x17, 0x3b, 0x73}};
inline constexpr GUID testHooksId = {
    0x735941a2, 0x3ee3, 0x495a, {0x8d, 0xa9, 0x97, 0x26, 0x27, 0x00, 0x30, 0x75}};
struct TestHooks : IUnknown {
  virtual HRESULT STDMETHODCALLTYPE UnregisterInstance(InstanceHandle) = 0;
  virtual HRESULT STDMETHODCALLTYPE TryGetDispatcherQueueForObject(InstanceHandle, void **) = 0;
};
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
  return found.empty() ? nullptr : found.front();
}
inline UINT dispatchMessage = RegisterWindowMessageW(L"Windhawk.SoundNative.Dispatch.2");
struct Job {
  std::function<void()> action;
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
    action();
    return true;
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
  return true;
}
struct Runtime;
inline bool BeginRelease();
inline bool FinishRelease();
inline bool Reconnect();
inline std::mutex runtimesMutex;
inline std::map<DWORD, Runtime *> runtimes;
struct Runtime {
  HWND window = nullptr;
  X::DispatcherTimer timer{nullptr};
  winrt::event_token tick{};
  winrt::com_ptr<TestHooks> release;
  std::set<InstanceHandle> pending;
  winrt::com_ptr<IXamlDiagnostics> diagnostics;
  winrt::com_ptr<IVisualTreeService3> properties;
  std::vector<winrt::weak_ref<X::FrameworkElement>> roots;
  std::vector<std::unique_ptr<SoundPage>> pages;
  bool busy = false;
  // XAML Diagnostics is released completely once the shell's L2Frame (it hosts
  // every Sound page) and the Quick Settings view are found. While any reference
  // to IXamlDiagnostics lives, XAML runs the whole process in its debugging mode
  // (global style/resource/live-tree tables with an entry for every element,
  // never pruned), and every Sound opening gets slower. The last reference
  // switches the mode off in XamlDiagnostics' destructor. New Sound pages are
  // taken from L2Frame.Content changes; the bridge reconnects if the frame goes
  // away and releases again once everything is found.
  winrt::weak_ref<X::FrameworkElement> l2frame;
  int64_t frameToken = 0;
  bool observing = true, releasing = false;
  void OnFrameContent(X::DependencyObject const &sender) {
    auto frame = sender.try_as<C::ContentControl>();
    if (!frame)
      return;
    auto page = frame.Content().try_as<X::FrameworkElement>();
    if (!page || Class(page).find(L"ControlCenter.FullScreenPage") == std::wstring::npos)
      return;
    WatchPage(page);
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
  // In Refresh (outside any report): release the diagnostics once everything is
  // found, reconnect if the frame is gone (the shell rebuilt the panel).
  void Observation() {
    auto frame = l2frame.get();
    bool ready = frame && !quickPages.empty();
    if (observing && ready && !releasing) {
      if (BeginRelease())
        releasing = true;
    } else if (!observing && !frame) {
      UnwatchFrame();
      if (Reconnect())
        observing = true;
    }
  }
  // End of Refresh, after the pending handles were returned: once the watcher has
  // unadvised, drop every reference, so XamlDiagnostics dies on the UI thread.
  void FinishObservation() {
    if (!releasing || !FinishRelease())
      return;
    release = nullptr;
    properties = nullptr;
    diagnostics = nullptr;
    releasing = false;
    observing = false;
  }
  // A reconnection (new XamlDiagnostics) reaches an existing Runtime.
  void Attach(winrt::com_ptr<TestHooks> const &hooks,
              winrt::com_ptr<IXamlDiagnostics> const &inspect) {
    if (diagnostics || !inspect)
      return;
    release = hooks;
    diagnostics = inspect;
    properties = diagnostics.as<IVisualTreeService3>();
  }
  std::vector<std::unique_ptr<QuickPage>> quickPages;
  struct PageWatch {
    winrt::weak_ref<X::FrameworkElement> page;
    winrt::event_token loaded, layout;
  };
  std::vector<PageWatch> pageWatches;
  std::shared_ptr<VolumePulse> volumePulse;
  bool pulseHooked = false, volumeDuringRefresh = false;
  static LRESULT CALLBACK VolumeProc(HWND window, UINT message, WPARAM w, LPARAM l,
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
      if (self->volumePulse)
        self->volumePulse->active = false;
      self->pulseHooked = false;
      WindhawkUtils::RemoveWindowSubclassFromAnyThread(window, VolumeProc);
    }
    return DefSubclassProc(window, message, w, l);
  }
  void EnsureVolumePulse() {
    if (pulseHooked && IsWindow(window))
      return;
    window = ThreadWindow(GetCurrentThreadId());
    if (!window)
      return;
    if (volumePulse)
      volumePulse->active = false;
    volumePulse = std::make_shared<VolumePulse>(window);
    pulseHooked = WindhawkUtils::SetWindowSubclassFromAnyThread(window, VolumeProc,
                                                                reinterpret_cast<DWORD_PTR>(this));
  }
  Runtime(winrt::com_ptr<TestHooks> hooks, winrt::com_ptr<IXamlDiagnostics> inspect)
      : window(ThreadWindow(GetCurrentThreadId())), release(std::move(hooks)),
        diagnostics(std::move(inspect)), properties(diagnostics.as<IVisualTreeService3>()) {
    EnsureVolumePulse();
    timer = X::DispatcherTimer();
    timer.Interval(std::chrono::milliseconds(200));
    tick = timer.Tick([this](auto &&, auto &&) { Refresh(); });
    timer.Start();
    std::lock_guard lock(runtimesMutex);
    runtimes[GetCurrentThreadId()] = this;
  }
  InstanceHandle Handle(IInspectable const &object) {
    InstanceHandle handle = 0;
    if (object && diagnostics &&
        SUCCEEDED(diagnostics->GetHandleFromIInspectable(
            reinterpret_cast<::IInspectable *>(winrt::get_abi(object)), &handle)) &&
        handle) {
      pending.insert(handle);
      return handle;
    }
    return 0;
  }
  IInspectable Property(IInspectable const &object, PCWSTR name) {
    if (!diagnostics || !properties)
      return nullptr;
    auto handle = Handle(object);
    unsigned index = 0;
    InstanceHandle value = 0;
    IInspectable result{nullptr};
    if (!handle || FAILED(properties->GetPropertyIndex(handle, name, &index)) ||
        FAILED(properties->GetProperty(handle, index, &value)) || !value)
      return nullptr;
    pending.insert(value);
    if (FAILED(diagnostics->GetIInspectableFromHandle(
            value, reinterpret_cast<::IInspectable **>(winrt::put_abi(result)))))
      return nullptr;
    return result;
  }
  bool WriteNumber(IInspectable const &object, PCWSTR name, double number) {
    auto next = NumberLike(Property(object, name), number);
    if (!next)
      return false;
    auto handle = Handle(object), value = Handle(next);
    unsigned index = 0;
    if (!handle || !value || FAILED(properties->GetPropertyIndex(handle, name, &index)))
      return false;
    auto hr = properties->SetProperty(handle, value, index);
    return SUCCEEDED(hr);
  }
  ModelAccess Access() {
    return {[this](auto const &object, PCWSTR name) { return Property(object, name); },
            [this](auto const &object, PCWSTR name, double value) {
              return WriteNumber(object, name, value);
            }};
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
    auto patch = std::make_unique<SoundPage>(page, window, Access(), [pulse] {
      if (pulse)
        pulse->Post();
    });
    if (patch->Apply()) {
      patch->PrepareInitialLayout();
      pages.push_back(std::move(patch));
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
    // Not here: WatchPage runs inside the tree-change callback. BeforeDisplay
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
  void Track(X::FrameworkElement const &element) {
    for (auto const &q : quickPages)
      q->Dirty();
    if (Class(element) == L"ControlCenter.ControlCenterView") {
      bool known = false;
      for (auto const &q : quickPages)
        if (q->View() == element) {
          known = true;
          break;
        }
      if (!known)
        try {
          quickPages.push_back(
              std::make_unique<QuickPage>(element, ThreadWindow(GetCurrentThreadId())));
          newestQuick = winrt::make_weak(element);
        } catch (...) {
          Wh_Log(L"QuickError %08X", winrt::to_hresult());
        }
    }
    if (auto root = element.XamlRoot())
      if (auto content = root.Content().try_as<X::FrameworkElement>()) {
        bool exists = false;
        for (auto const &weak : roots)
          if (weak.get() == content) {
            exists = true;
            break;
          }
        if (!exists)
          roots.push_back(winrt::make_weak(content));
      }
    if (element.Name() == L"L2Frame")
      WatchFrame(element);
    auto name = element.Name();
    if (name == L"OutputGroupTitle" || name == L"AppVolumeLevel" ||
        Class(element).find(L"ControlCenter.FullScreenPage") != std::wstring::npos) {
      auto page = element;
      while (page && Class(page).find(L"ControlCenter.FullScreenPage") == std::wstring::npos)
        page = Parent(page);
      if (page) {
        WatchPage(page);
        if (name == L"AppVolumeLevel")
          for (auto const &patch : pages)
            if (patch->Page() == page)
              patch->MarkRowsDirty();
      }
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
      if (observing)
        for (auto const &weak : std::vector(roots)) {
          auto root = weak.get();
          if (!root)
            continue;
          std::vector<X::FrameworkElement> candidates;
          Find(root, [&](auto const &e) {
            if (e.Name() == L"OutputGroupTitle") {
              auto page = e;
              while (page &&
                     Class(page).find(L"ControlCenter.FullScreenPage") == std::wstring::npos)
                page = Parent(page);
              if (page && std::find(candidates.begin(), candidates.end(), page) == candidates.end())
                candidates.push_back(page);
            }
            return false;
          });
          for (auto const &page : candidates) {
            WatchPage(page);
            PreparePage(page, false);
          }
        }
      for (auto const &q : quickPages)
        q->Refresh();
      Observation();
    } catch (...) {
      Wh_Log(L"Native layout: %08X", winrt::to_hresult());
    }
    if (timer) {
      auto interval = std::chrono::milliseconds(pages.empty() ? 1000 : 200);
      if (timer.Interval() != winrt::Windows::Foundation::TimeSpan(interval))
        timer.Interval(interval);
    }
    auto handles = std::move(pending);
    pending.clear();
    if (release)
      for (auto handle : handles)
        if (handle)
          release->UnregisterInstance(handle);
    FinishObservation();
    busy = false;
    if (volumeDuringRefresh) {
      volumeDuringRefresh = false;
      if (volumePulse)
        volumePulse->Post();
    }
  }
  void Stop() {
    UnwatchFrame();
    modelMembers.clear();
    for (auto &q : quickPages)
      q->Restore();
    quickPages.clear();
    for (auto const &watch : pageWatches)
      if (auto page = watch.page.get()) {
        page.Loaded(watch.loaded);
        page.LayoutUpdated(watch.layout);
      }
    pageWatches.clear();
    if (volumePulse)
      volumePulse->active = false;
    if (pulseHooked) {
      WindhawkUtils::RemoveWindowSubclassFromAnyThread(window, VolumeProc);
      pulseHooked = false;
    }
    timer.Stop();
    timer.Tick(tick);
    for (auto &page : pages)
      page->Restore();
    pages.clear();
    if (release)
      for (auto id : pending)
        release->UnregisterInstance(id);
    pending.clear();
    roots.clear();
    timer = nullptr;
    properties = nullptr;
    diagnostics = nullptr;
    release = nullptr;
  }
};
inline thread_local std::unique_ptr<Runtime> runtime;
inline thread_local bool compositionDisabled = false;
class Watcher : public winrt::implements<Watcher, IVisualTreeServiceCallback2, winrt::non_agile> {
  winrt::com_ptr<IXamlDiagnostics> diagnostics_;
  winrt::com_ptr<TestHooks> hooks_;
  HANDLE worker_ = nullptr;

public:
  explicit Watcher(IUnknown *site) {
    winrt::com_ptr<IUnknown> unknown;
    unknown.copy_from(site);
    diagnostics_ = unknown.as<IXamlDiagnostics>();
    diagnostics_->QueryInterface(testHooksId, hooks_.put_void());
  }
  void Advise() { Switch(true); }
  // Last step of a release (UI thread, the watcher has unadvised).
  void Drop() {
    diagnostics_ = nullptr;
    hooks_ = nullptr;
  }
  std::atomic<bool> advised = false;
  void Join() {
    if (worker_) {
      WaitForSingleObject(worker_, INFINITE);
      CloseHandle(worker_);
      worker_ = nullptr;
    }
  }
  // Reports on/off without dropping the diagnostics connection. Runs on its own
  // thread, never inside a report; a new Advise re-reports the whole tree.
  // Never waits: Advise re-reports the tree through the UI thread, which may be
  // the caller. A switch while the previous one still runs is retried later.
  bool Busy() {
    if (!worker_)
      return false;
    if (WaitForSingleObject(worker_, 0) == WAIT_OBJECT_0) {
      CloseHandle(worker_);
      worker_ = nullptr;
      return false;
    }
    return true;
  }
  bool Switch(bool on) {
    if (Busy())
      return false;
    if (on == advised)
      return true;
    AddRef();
    worker_ = CreateThread(
        nullptr, 0,
        on ? static_cast<LPTHREAD_START_ROUTINE>([](void *p) -> DWORD {
          auto self = static_cast<Watcher *>(p);
          compositionDisabled = true;
          auto hr = self->diagnostics_.as<IVisualTreeService3>()->AdviseVisualTreeChange(self);
          compositionDisabled = false;
          self->advised = SUCCEEDED(hr);
          self->Release();
          return 0;
        })
           : static_cast<LPTHREAD_START_ROUTINE>([](void *p) -> DWORD {
               auto self = static_cast<Watcher *>(p);
               self->diagnostics_.as<IVisualTreeService3>()->UnadviseVisualTreeChange(self);
               self->advised = false;
               self->Release();
               return 0;
             }),
        this, 0, nullptr);
    if (!worker_) {
      Release();
      return false;
    }
    return true;
  }
  void Stop() {
    Join();
    if (advised) {
      diagnostics_.as<IVisualTreeService3>()->UnadviseVisualTreeChange(this);
      advised = false;
    }
  }
  HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation relation, VisualElement element,
                                               VisualMutationType mutation) noexcept override {
    try {
      if (stopping || !diagnostics_)
        return S_OK;
      if (!runtime)
        runtime = std::make_unique<Runtime>(hooks_, diagnostics_);
      else
        runtime->Attach(hooks_, diagnostics_);
      if (mutation == Add) {
        IInspectable value{nullptr};
        if (SUCCEEDED(diagnostics_->GetIInspectableFromHandle(
                element.Handle, reinterpret_cast<::IInspectable **>(winrt::put_abi(value)))))
          if (auto e = value.try_as<X::FrameworkElement>())
            runtime->Track(e);
      }
      // Every handle a report names is released, the parent of a removal too: the
      // service creates (and pins) a runtime object for it when none is cached, and
      // a pinned parent would hold old page subtrees and their bindings.
      runtime->pending.insert(relation.Parent);
      runtime->pending.insert(element.Handle);
    } catch (...) {
      Wh_Log(L"XAML observation: %08X", winrt::to_hresult());
    }
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle, VisualElementState,
                                                  LPCWSTR) noexcept override {
    return S_OK;
  }
};
inline std::mutex watcherMutex;
inline winrt::com_ptr<Watcher> watcher, retiredWatcher;
// Release in two steps: reports off on the watcher's own thread (never inside a
// report), then FinishRelease in a later Refresh drops the watcher's references.
inline bool BeginRelease() {
  std::lock_guard lock(watcherMutex);
  if (retiredWatcher || !watcher || !watcher->Switch(false))
    return false;
  retiredWatcher = std::move(watcher);
  return true;
}
inline bool FinishRelease() {
  std::lock_guard lock(watcherMutex);
  if (!retiredWatcher)
    return true;
  if (retiredWatcher->Busy())
    return false;
  retiredWatcher->Drop();
  retiredWatcher = nullptr;
  return true;
}
// The site is not kept: only the watcher and the Runtime hold XamlDiagnostics,
// so releasing them really destroys it (see Runtime::Observation).
struct Tap : winrt::implements<Tap, IObjectWithSite, winrt::non_agile> {
  HRESULT STDMETHODCALLTYPE SetSite(IUnknown *value) noexcept override {
    try {
      std::lock_guard lock(watcherMutex);
      if (watcher) {
        watcher->Stop();
        watcher = nullptr;
      }
      if (value && !stopping) {
        FreeLibrary(module);
        watcher = winrt::make_self<Watcher>(value);
        watcher->Advise();
      }
      return S_OK;
    } catch (...) {
      return winrt::to_hresult();
    }
  }
  HRESULT STDMETHODCALLTYPE GetSite(REFIID, void **result) noexcept override {
    if (result)
      *result = nullptr;
    return E_FAIL;
  }
};
struct Factory : winrt::implements<Factory, IClassFactory, winrt::non_agile> {
  HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID iid,
                                           void **result) noexcept override {
    if (!result)
      return E_POINTER;
    *result = nullptr;
    if (outer)
      return CLASS_E_NOAGGREGATION;
    try {
      return winrt::make<Tap>().as(iid, result);
    } catch (...) {
      return winrt::to_hresult();
    }
  }
  HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override { return S_OK; }
};
inline HANDLE connectionWorker = nullptr, stopEvent = nullptr;
inline std::atomic<bool> connecting = false;
inline DWORD WINAPI Connect(void *) {
  auto xaml = LoadLibraryExW(L"Windows.UI.Xaml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!xaml) {
    connecting = false;
    return 0;
  }
  auto initialize = reinterpret_cast<decltype(&InitializeXamlDiagnosticsEx)>(
      GetProcAddress(xaml, "InitializeXamlDiagnosticsEx"));
  WCHAR path[32768]{};
  GetModuleFileNameW(module, path, 32768);
  HRESULT result = E_NOINTERFACE;
  if (initialize)
    for (unsigned retry = 0; retry < 60 && !stopping; retry++) {
      for (int i = 1; i <= 10000; i++) {
        WCHAR name[64];
        swprintf_s(name, L"VisualDiagConnection%d", i);
        result = initialize(name, GetCurrentProcessId(), L"", path, tapClsid, nullptr);
        if (result != HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
          break;
      }
      if (SUCCEEDED(result) || result != HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
        break;
      if (WaitForSingleObject(stopEvent, 500) == WAIT_OBJECT_0)
        break;
    }
  FreeLibrary(xaml);
  connecting = false;
  return 0;
}
inline void StartBridge() {
  if (stopping || connecting.exchange(true))
    return;
  {
    std::lock_guard lock(watcherMutex);
    if (watcher) {
      connecting = false;
      return;
    }
  }
  if (connectionWorker) {
    WaitForSingleObject(connectionWorker, INFINITE);
    CloseHandle(connectionWorker);
  }
  connectionWorker = CreateThread(nullptr, 0, Connect, nullptr, 0, nullptr);
  if (!connectionWorker)
    connecting = false;
}
// From Refresh on the UI thread: never waits for a connection still running.
inline bool Reconnect() {
  if (stopping || connecting)
    return false;
  if (connectionWorker && WaitForSingleObject(connectionWorker, 0) != WAIT_OBJECT_0)
    return false;
  StartBridge();
  return true;
}
inline void StopBridge() {
  if (stopEvent)
    SetEvent(stopEvent);
  if (connectionWorker) {
    WaitForSingleObject(connectionWorker, INFINITE);
    CloseHandle(connectionWorker);
    connectionWorker = nullptr;
  }
  {
    std::lock_guard lock(watcherMutex);
    if (watcher) {
      watcher->Stop();
      watcher = nullptr;
    }
    if (retiredWatcher) {
      retiredWatcher->Stop();
      retiredWatcher = nullptr;
    }
  }
  std::vector<std::pair<DWORD, Runtime *>> copy;
  {
    std::lock_guard lock(runtimesMutex);
    for (auto pair : runtimes)
      copy.push_back(pair);
  }
  for (auto [thread, state] : copy)
    if (auto window = state->window ? state->window : ThreadWindow(thread))
      Dispatch(window, [] {
        if (runtime) {
          runtime->Stop();
          runtime.reset();
        }
      });
  {
    std::lock_guard lock(runtimesMutex);
    runtimes.clear();
  }
  if (stopEvent) {
    CloseHandle(stopEvent);
    stopEvent = nullptr;
  }
}
} // namespace native
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdll-attribute-on-redeclaration"
extern "C" __declspec(dllexport) HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid,
                                                                  void **result) {
  if (!result)
    return E_POINTER;
  *result = nullptr;
  if (clsid != native::tapClsid)
    return CLASS_E_CLASSNOTAVAILABLE;
  try {
    return winrt::make<native::Factory>().as(iid, result);
  } catch (...) {
    return winrt::to_hresult();
  }
}
extern "C" __declspec(dllexport) HRESULT WINAPI DllCanUnloadNow() {
  return winrt::get_module_lock() ? S_FALSE : S_OK;
}
#pragma clang diagnostic pop

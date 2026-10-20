#pragma once
#include "bridge.h"
namespace native {
// Discovery uses only COM produce<> entry points, never the implementation's
// partially constructed `this`. Pattern validated in ControlCenter.pdb.
using Focus = int(WINAPI *)(void *, void *);
using Connect = int(WINAPI *)(void *, int, void *);
inline Focus originalViewFocus = nullptr, originalPageFocus = nullptr;
inline Connect originalViewConnect = nullptr, originalPageConnect = nullptr;
inline std::atomic<int> discoveryState{0}; // 0 waiting, 1 installing, 2 ready, 3 failed
inline std::mutex discoveryMutex;
inline std::mutex discoveryInstallMutex;
inline thread_local bool insideDiscovery = false;
inline void Discovered(void *object, bool deferred) noexcept {
  if (!object || stopping || insideDiscovery)
    return;
  insideDiscovery = true;
  try {
    X::FrameworkElement element{nullptr};
    if (SUCCEEDED(static_cast<::IUnknown *>(object)->QueryInterface(
            winrt::guid_of<X::FrameworkElement>(), winrt::put_abi(element))) &&
        element) {
      {
        // Unload crosses this lock before taking the runtime snapshot. It is
        // never held while the engine waits for work on the UI thread.
        std::lock_guard lock(discoveryMutex);
        if (!stopping && !runtime)
          runtime = std::make_unique<Runtime>(element);
      }
      if (!stopping && runtime) {
        if (deferred)
          runtime->QueueDiscovery(element);
        else
          runtime->Discover(element);
      }
    }
  } catch (...) {
    Wh_Log(L"ControlCenter symbol callback: %08X", winrt::to_hresult());
  }
  insideDiscovery = false;
}
inline int WINAPI ViewFocus(void *self, void *args) {
  auto result = originalViewFocus(self, args);
  Discovered(self, false);
  return result;
}
inline int WINAPI PageFocus(void *self, void *args) {
  auto result = originalPageFocus(self, args);
  Discovered(self, false);
  return result;
}
inline int WINAPI ViewConnect(void *self, int id, void *target) {
  auto result = originalViewConnect(self, id, target);
  Discovered(target, true);
  return result;
}
inline int WINAPI PageConnect(void *self, int id, void *target) {
  auto result = originalPageConnect(self, id, target);
  Discovered(target, true);
  return result;
}
inline bool HookControlCenter(HMODULE controlCenter, bool applyNow) {
  std::lock_guard installLock(discoveryInstallMutex);
  if (stopping)
    return false;
  int expected = 0;
  if (!discoveryState.compare_exchange_strong(expected, 1))
    return expected != 3;
  WindhawkUtils::SYMBOL_HOOK controlCenterDllHooks[] = {
      {{LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::ControlCenter::implementation::ControlCenterView,struct winrt::Windows::UI::Xaml::Controls::IControlOverrides>::OnGotFocus(void *))"},
       &originalViewFocus,
       ViewFocus},
      {{LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::ControlCenter::implementation::ControlCenterView,struct winrt::Windows::UI::Xaml::Markup::IComponentConnector>::Connect(int,void *))"},
       &originalViewConnect,
       ViewConnect,
       true},
      {{LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::ControlCenter::implementation::FullScreenPage,struct winrt::Windows::UI::Xaml::Controls::IControlOverrides>::OnGotFocus(void *))"},
       &originalPageFocus,
       PageFocus,
       true},
      {{LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::ControlCenter::implementation::FullScreenPage,struct winrt::Windows::UI::Xaml::Markup::IComponentConnector>::Connect(int,void *))"},
       &originalPageConnect,
       PageConnect,
       true},
  };
  bool ok = WindhawkUtils::HookSymbols(controlCenter, controlCenterDllHooks,
                                       ARRAYSIZE(controlCenterDllHooks));
  if (ok && applyNow)
    ok = Wh_ApplyHookOperations();
  discoveryState = ok ? 2 : 3;
  Wh_Log(L"ControlCenter.dll discovery hooks: %s", ok ? L"ready" : L"failed");
  return ok;
}
using HostLoad = decltype(&LoadLibraryExW);
inline HostLoad originalHostLoad = nullptr;
inline HMODULE WINAPI HostLoaded(LPCWSTR path, HANDLE file, DWORD flags) {
  auto result = originalHostLoad(path, file, flags);
  if (result && !stopping && discoveryState == 0 &&
      !(flags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE |
                 LOAD_LIBRARY_AS_IMAGE_RESOURCE)) &&
      result == GetModuleHandleW(L"ControlCenter.dll"))
    HookControlCenter(result, true);
  return result;
}
inline bool InitializeHost() {
  auto kernel = GetModuleHandleW(L"kernelbase.dll");
  auto load =
      kernel ? reinterpret_cast<HostLoad>(GetProcAddress(kernel, "LoadLibraryExW")) : nullptr;
  if (!load || !WindhawkUtils::SetFunctionHook(load, HostLoaded, &originalHostLoad))
    return false;
  if (auto controlCenter = GetModuleHandleW(L"ControlCenter.dll"))
    return HookControlCenter(controlCenter, false);
  Wh_Log(L"Waiting for ControlCenter.dll to be loaded by the shell");
  return true;
}
inline void AfterHostInit() {
  // Close the race where the DLL loaded before the load hook was armed.
  if (!stopping)
    if (auto controlCenter = GetModuleHandleW(L"ControlCenter.dll"))
      HookControlCenter(controlCenter, true);
}
inline void StopHost() {
  {
    std::lock_guard installLock(discoveryInstallMutex);
    // Finish any symbol installation before the module can be unloaded.
  }
  {
    std::lock_guard lock(discoveryMutex);
    // stopping is already set by Wh_ModUninit; wait only for runtime creation.
  }
  StopBridge();
}
} // namespace native

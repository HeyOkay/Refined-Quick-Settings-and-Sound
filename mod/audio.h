#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <mmreg.h> // WAVEFORMATEX for the endpoint policy interface
#include <powrprof.h>
#include <powersetting.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#include <shlwapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include "model.h"
namespace sound {
// Display-on and resume-from-sleep notifications (power callbacks on a system
// thread). DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS and DEVICE_NOTIFY_CALLBACK (2) are
// declared here: the mod's MinGW headers lack them.
class PowerWake {
  struct Subscribe {
    ULONG(CALLBACK *Callback)(PVOID, ULONG, PVOID);
    PVOID Context;
  };
  static constexpr DWORD notifyCallback = 2;
  static constexpr GUID displayState = {
      0x6fe69556, 0x704a, 0x47a0, {0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47}};
  std::function<void()> on_;
  Subscribe params_{};
  HPOWERNOTIFY display_ = nullptr, resume_ = nullptr;
  static ULONG CALLBACK Changed(PVOID context, ULONG type, PVOID setting) {
    bool wake = type == PBT_APMRESUMEAUTOMATIC;
    if (type == PBT_POWERSETTINGCHANGE && setting) {
      auto s = static_cast<POWERBROADCAST_SETTING *>(setting);
      wake = IsEqualGUID(s->PowerSetting, displayState) && s->DataLength >= sizeof(DWORD) &&
             *reinterpret_cast<DWORD const *>(s->Data) == 1;
    }
    if (wake && context)
      try {
        static_cast<PowerWake *>(context)->on_();
      } catch (...) {
      }
    return 0;
  }

public:
  explicit PowerWake(std::function<void()> on) : on_(std::move(on)) {
    params_.Callback = Changed;
    params_.Context = this;
    if (PowerSettingRegisterNotification(&displayState, notifyCallback, &params_, &display_) !=
        ERROR_SUCCESS)
      display_ = nullptr;
    if (PowerRegisterSuspendResumeNotification(notifyCallback, &params_, &resume_) !=
        ERROR_SUCCESS)
      resume_ = nullptr;
  }
  PowerWake(PowerWake const &) = delete;
  PowerWake &operator=(PowerWake const &) = delete;
  ~PowerWake() {
    if (display_)
      PowerSettingUnregisterNotification(display_);
    if (resume_)
      PowerUnregisterSuspendResumeNotification(resume_);
  }
};
// All COM audio objects belong to one MTA thread. The UI exchanges plain data.
class Audio final {
  struct VolumeFeed {
    std::mutex mutex;
    std::wstring id;
    float value = 0;
    bool muted = false, available = false, active = true;
    uint64_t revision = 0, noticeAt = 0;
    std::function<void()> publish;
  };
  struct VolumeNotify : winrt::implements<VolumeNotify, IAudioEndpointVolumeCallback> {
    std::shared_ptr<VolumeFeed> feed;
    std::wstring id;
    VolumeNotify(std::shared_ptr<VolumeFeed> value, std::wstring endpoint)
        : feed(std::move(value)), id(std::move(endpoint)) {}
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA data) noexcept override {
      if (!data)
        return E_POINTER;
      try {
        std::function<void()> publish;
        {
          std::lock_guard lock(feed->mutex);
          if (!feed->active || feed->id != id)
            return S_OK;
          feed->value = data->fMasterVolume;
          feed->muted = data->bMuted;
          feed->available = true;
          feed->revision++;
          feed->noticeAt = GetTickCount64();
          publish = feed->publish;
        }
        if (publish)
          publish();
        return S_OK;
      } catch (...) {
        return winrt::to_hresult();
      }
    }
  };
  struct Wake {
    std::mutex mutex;
    std::condition_variable cv;
    bool changed = true;
    void Signal() {
      std::lock_guard lock(mutex);
      changed = true;
      cv.notify_one();
    }
  };
  struct InputVolumeNotify : winrt::implements<InputVolumeNotify, IAudioEndpointVolumeCallback> {
    std::shared_ptr<Wake> wake;
    explicit InputVolumeNotify(std::shared_ptr<Wake> value) : wake(std::move(value)) {}
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA) noexcept override {
      wake->Signal();
      return S_OK;
    }
  };
  struct Notify : winrt::implements<Notify, IMMNotificationClient> {
    std::shared_ptr<Wake> wake;
    explicit Notify(std::shared_ptr<Wake> value) : wake(std::move(value)) {}
    void Signal() {
      wake->Signal();
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override {
      Signal();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
      Signal();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
      Signal();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
      Signal();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
      Signal();
      return S_OK;
    }
  };
  // Undocumented endpoint policy ABI, limited to the known interface and checked HRESULTs.
  struct Policy : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(LPCWSTR, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(LPCWSTR, BOOL, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(LPCWSTR, WAVEFORMATEX *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(LPCWSTR, BOOL, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(LPCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(LPCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(LPCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(LPCWSTR, const PROPERTYKEY *,
                                                       PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(LPCWSTR, const PROPERTYKEY *,
                                                       PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(LPCWSTR, ERole) = 0;
  };
  std::shared_ptr<Wake> wake_ = std::make_shared<Wake>();
  std::thread worker_;
  std::atomic<bool> stop_{false};
  std::mutex mutex_;
  std::deque<Command> commands_;
  Snapshot snapshot_;
  std::function<void()> publish_;
  winrt::com_ptr<IMMDeviceEnumerator> enumerator_;
  winrt::com_ptr<IAudioEndpointVolume> master_;
  winrt::com_ptr<IAudioEndpointVolume> inputVolume_;
  winrt::com_ptr<InputVolumeNotify> inputVolumeObserver_;
  std::atomic<bool> inputVolumeEnabled_{false};
  std::wstring inputVolumeId_;
  bool inputVolumeRejected_ = false;
  InputVolumeRetry inputVolumeRetry_;
  std::shared_ptr<VolumeFeed> volumeFeed_ = std::make_shared<VolumeFeed>();
  winrt::com_ptr<VolumeNotify> volumeObserver_;
  std::map<std::wstring, SavedVolume> masters_;
  std::wstring masterId_, error_;
  unsigned long long revision_ = 0, applied_ = 0;
  std::atomic<unsigned long long> next_{0};
  static std::wstring Id(IMMDevice *device) {
    LPWSTR raw = nullptr;
    if (!device || FAILED(device->GetId(&raw)))
      return {};
    std::wstring result(raw);
    CoTaskMemFree(raw);
    return result;
  }
  static std::wstring Friendly(IMMDevice *device) {
    winrt::com_ptr<IPropertyStore> props;
    PROPVARIANT value{};
    std::wstring name = Text(L"Audio device", L"Аудиоустройство", L"Dispositivo de audio", L"Audiogerät", L"Dispositivo de áudio", L"音频设备");
    constexpr PROPERTYKEY friendlyKey = {
        {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, props.put())) &&
        SUCCEEDED(props->GetValue(friendlyKey, &value))) {
      if (value.vt == VT_LPWSTR)
        name = value.pwszVal;
      PropVariantClear(&value);
    }
    return name;
  }
  std::wstring DefaultDevice(EDataFlow flow) {
    winrt::com_ptr<IMMDevice> current;
    auto result = enumerator_->GetDefaultAudioEndpoint(flow, eMultimedia, current.put());
    if (result == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
      return {};
    winrt::check_hresult(result);
    return Id(current.get());
  }
  std::vector<Device> Devices(EDataFlow flow, std::wstring const &defaultId) {
    std::vector<Device> result;
    winrt::com_ptr<IMMDeviceCollection> devices;
    winrt::check_hresult(enumerator_->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, devices.put()));
    UINT count = 0;
    winrt::check_hresult(devices->GetCount(&count));
    for (UINT i = 0; i < count; i++) {
      winrt::com_ptr<IMMDevice> device;
      if (FAILED(devices->Item(i, device.put())))
        continue;
      auto id = Id(device.get());
      result.push_back({id, Friendly(device.get()), id == defaultId});
    }
    return result;
  }
  void BindMaster(std::wstring const &id) {
    if (id == masterId_ && master_)
      return;
    if (master_ && volumeObserver_)
      master_->UnregisterControlChangeNotify(volumeObserver_.get());
    volumeObserver_ = nullptr;
    master_ = nullptr;
    masterId_ = id;
    {
      std::lock_guard lock(volumeFeed_->mutex);
      volumeFeed_->id = id;
      volumeFeed_->available = false;
      volumeFeed_->revision++;
    }
    if (id.empty())
      return;
    winrt::com_ptr<IMMDevice> device;
    if (SUCCEEDED(enumerator_->GetDevice(id.c_str(), device.put())))
      device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr,
                       master_.put_void());
    if (master_) {
      volumeObserver_ = winrt::make_self<VolumeNotify>(volumeFeed_, id);
      if (FAILED(master_->RegisterControlChangeNotify(volumeObserver_.get())))
        volumeObserver_ = nullptr;
      uint64_t revision;
      {
        std::lock_guard lock(volumeFeed_->mutex);
        revision = volumeFeed_->revision;
      }
      float value = 0;
      BOOL muted = FALSE;
      if (SUCCEEDED(master_->GetMasterVolumeLevelScalar(&value)) &&
          SUCCEEDED(master_->GetMute(&muted))) {
        std::lock_guard lock(volumeFeed_->mutex);
        if (volumeFeed_->revision == revision) {
          volumeFeed_->value = value;
          volumeFeed_->muted = muted;
          volumeFeed_->available = bool(volumeObserver_);
        }
      }
    }
  }
  void SetDefault(std::wstring const &id) {
    const CLSID clsid = {
        0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};
    const IID iid = {0xf8679f50, 0x850a, 0x41cf, {0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8}};
    winrt::com_ptr<Policy> policy;
    winrt::check_hresult(
        CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, iid, policy.put_void()));
    winrt::com_ptr<IMMDevice> selected;
    winrt::check_hresult(enumerator_->GetDevice(id.c_str(), selected.put()));
    EDataFlow flow;
    winrt::check_hresult(selected.as<IMMEndpoint>()->GetDataFlow(&flow));
    std::vector<std::pair<ERole, std::wstring>> before;
    for (auto role : {eConsole, eMultimedia, eCommunications}) {
      winrt::com_ptr<IMMDevice> device;
      enumerator_->GetDefaultAudioEndpoint(flow, role, device.put());
      before.emplace_back(role, Id(device.get()));
    }
    size_t completed = 0;
    for (auto const &[role, previous] : before) {
      auto result = policy->SetDefaultEndpoint(id.c_str(), role);
      if (FAILED(result)) {
        for (size_t i = 0; i < completed; i++)
          if (!before[i].second.empty())
            policy->SetDefaultEndpoint(before[i].second.c_str(), before[i].first);
        winrt::check_hresult(result);
      }
      ++completed;
    }
  }
  void UnbindInputVolume() {
    if (inputVolume_ && inputVolumeObserver_)
      inputVolume_->UnregisterControlChangeNotify(inputVolumeObserver_.get());
    inputVolumeObserver_ = nullptr;
    inputVolume_ = nullptr;
  }
  InputVolumeProbe FailedInputVolumeRead(HRESULT result) {
    UnbindInputVolume();
    inputVolumeRejected_ = UnsupportedInputVolume(result);
    return inputVolumeRejected_ ? InputVolumeProbe::Idle : InputVolumeProbe::Failed;
  }
  InputVolumeProbe ReadInputVolume(Snapshot &state) {
    auto id = inputVolumeEnabled_ ? state.inputId : std::wstring{};
    if (id != inputVolumeId_) {
      UnbindInputVolume();
      inputVolumeId_ = id;
      inputVolumeRejected_ = false;
      inputVolumeRetry_ = {};
    }
    if (id.empty() || inputVolumeRejected_) {
      UnbindInputVolume();
      return InputVolumeProbe::Idle;
    }
    if (!inputVolume_) {
      winrt::com_ptr<IMMDevice> device;
      if (FAILED(enumerator_->GetDevice(id.c_str(), device.put())) || !device)
        return InputVolumeProbe::Failed;
      auto result = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr,
                                     inputVolume_.put_void());
      if (FAILED(result))
        return FailedInputVolumeRead(result);
      if (!inputVolume_)
        return InputVolumeProbe::Failed;
      inputVolumeObserver_ = winrt::make_self<InputVolumeNotify>(wake_);
      if (FAILED(inputVolume_->RegisterControlChangeNotify(inputVolumeObserver_.get())))
        inputVolumeObserver_ = nullptr;
    }
    auto result = inputVolume_->GetMasterVolumeLevelScalar(&state.inputVolume);
    if (FAILED(result) || !std::isfinite(state.inputVolume))
      return FailedInputVolumeRead(result);
    float low, high, step;
    // Software endpoint volume also works for virtual devices. A fixed range
    // means the device offers no adjustment, regardless of its form factor.
    state.inputVolumeAvailable =
        FAILED(inputVolume_->GetVolumeRange(&low, &high, &step)) || high > low;
    BOOL muted = FALSE;
    state.inputMuteAvailable = SUCCEEDED(inputVolume_->GetMute(&muted));
    state.inputMuted = muted;
    return state.inputVolumeAvailable && !inputVolumeObserver_ ? InputVolumeProbe::Poll
                                                              : InputVolumeProbe::Idle;
  }
  void SetInputVolume(Command const &cmd, float value) {
    // A queued drag or click can outlive a default device change. Keep both
    // operations tied to the endpoint that was visible when the event fired.
    if (!inputVolumeEnabled_ || !inputVolume_ || cmd.id != inputVolumeId_ ||
        cmd.id != DefaultDevice(eCapture))
      return;
    BOOL muted = FALSE;
    if (cmd.action == Action::InputMute) {
      winrt::check_hresult(inputVolume_->GetMute(&muted));
      winrt::check_hresult(inputVolume_->SetMute(!muted, nullptr));
      return;
    }
    bool canMute = SUCCEEDED(inputVolume_->GetMute(&muted));
    // Keep the last gain when muting at zero, if the endpoint supports mute.
    if (value <= 0 && canMute && SUCCEEDED(inputVolume_->SetMute(TRUE, nullptr)))
      return;
    auto result = inputVolume_->SetMasterVolumeLevelScalar(value, nullptr);
    if (UnsupportedInputVolume(result))
      inputVolumeRejected_ = true;
    winrt::check_hresult(result);
    if (value > 0 && canMute && muted)
      winrt::check_hresult(inputVolume_->SetMute(FALSE, nullptr));
  }
  void Execute(Command const &cmd) {
    error_.clear();
    auto value = Clamp(cmd.value);
    if (cmd.action == Action::ToggleEndpointMute) {
      if (!master_)
        throw winrt::hresult_error(E_NOTFOUND, L"Output device unavailable");
      BOOL muted = FALSE;
      winrt::check_hresult(master_->GetMute(&muted));
      winrt::check_hresult(master_->SetMute(!muted, nullptr));
    } else if (cmd.action == Action::MasterVolume || cmd.action == Action::MasterMute) {
      if (!master_)
        throw winrt::hresult_error(E_NOTFOUND, L"Output device unavailable");
      float before = 0;
      BOOL muted = FALSE;
      winrt::check_hresult(master_->GetMasterVolumeLevelScalar(&before));
      winrt::check_hresult(master_->GetMute(&muted));
      auto &saved = masters_[masterId_];
      saved.Observe(before, muted);
      if (cmd.action == Action::MasterMute)
        value = saved.Toggle(before, muted);
      winrt::check_hresult(master_->SetMasterVolumeLevelScalar(value, nullptr));
      winrt::check_hresult(master_->SetMute(value <= 0, nullptr));
    } else if (cmd.action == Action::InputVolume || cmd.action == Action::InputMute) {
      SetInputVolume(cmd, value);
    } else if (cmd.action == Action::Input) {
      winrt::com_ptr<IMMDevice> target;
      winrt::check_hresult(enumerator_->GetDevice(cmd.id.c_str(), target.put()));
      DWORD status = 0;
      winrt::check_hresult(target->GetState(&status));
      if (!(status & DEVICE_STATE_ACTIVE))
        throw winrt::hresult_error(E_NOTFOUND, L"Device disconnected");
      auto endpoint = target.as<IMMEndpoint>();
      EDataFlow flow;
      winrt::check_hresult(endpoint->GetDataFlow(&flow));
      if (flow != eCapture)
        throw winrt::hresult_error(E_INVALIDARG);
      SetDefault(cmd.id);
    }
  }
  std::chrono::seconds Read() {
    Snapshot state;
    state.revision = ++revision_;
    state.applied = applied_;
    state.error = error_;
    state.outputId = DefaultDevice(eRender);
    BindMaster(state.outputId);
    if (master_) {
      BOOL mute = FALSE;
      state.masterAvailable = SUCCEEDED(master_->GetMasterVolumeLevelScalar(&state.master)) &&
                              SUCCEEDED(master_->GetMute(&mute));
      state.muted = mute;
      masters_[masterId_].Observe(state.master, state.muted);
      // An invalidated endpoint keeps failing: it is opened again on the next
      // read (the next notification).
      if (!state.masterAvailable) {
        if (volumeObserver_)
          master_->UnregisterControlChangeNotify(volumeObserver_.get());
        volumeObserver_ = nullptr;
        master_ = nullptr;
        std::lock_guard lock(volumeFeed_->mutex);
        volumeFeed_->available = false;
      }
    }
    bool retry = !state.outputId.empty() && !state.masterAvailable;
    // The input list and the endpoint volume have independent failure modes.
    // Publish the current volume even if an endpoint collection is unavailable.
    bool inputIdKnown = false;
    for (auto flow : {eRender, eCapture})
      try {
        if (flow == eRender)
          state.output = Devices(flow, state.outputId);
        else {
          state.inputId = DefaultDevice(flow);
          inputIdKnown = true;
          state.input = Devices(flow, state.inputId);
        }
      } catch (winrt::hresult_error const &) {
        retry = true;
        std::lock_guard lock(mutex_);
        if (flow == eRender && snapshot_.outputId == state.outputId)
          state.output = snapshot_.output;
        else if (flow == eCapture) {
          if (!inputIdKnown)
            state.inputId = snapshot_.inputId;
          if (snapshot_.inputId == state.inputId)
            state.input = snapshot_.input;
        }
      }
    auto inputProbe = inputIdKnown || !inputVolumeEnabled_ ? ReadInputVolume(state)
                                                         : InputVolumeProbe::Idle;
    auto inputDelay = inputVolumeRetry_.Delay(inputProbe);
    // The shell's own lists serve output devices, application sessions and
    // spatial sound; this worker serves the added input selector, the master
    // volume and the tray.
    {
      std::lock_guard lock(mutex_);
      snapshot_ = std::move(state);
    }
    if (publish_)
      publish_();
    return retry ? std::chrono::seconds{1} : inputDelay;
  }
  void Run() noexcept {
    bool apartment = false;
    try {
      winrt::init_apartment(winrt::apartment_type::multi_threaded);
      apartment = true;
      winrt::check_hresult(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                            CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
                                            enumerator_.put_void()));
      auto observer = winrt::make_self<Notify>(wake_);
      winrt::check_hresult(enumerator_->RegisterEndpointNotificationCallback(observer.get()));
      while (!stop_) {
        std::deque<Command> pending;
        {
          std::lock_guard lock(mutex_);
          pending.swap(commands_);
        }
        for (auto const &cmd : pending) {
          try {
            Execute(cmd);
          } catch (winrt::hresult_error const &ex) {
            WCHAR code[32];
            swprintf_s(code, L" (0x%08X)", unsigned(ex.code().value));
            error_ = std::wstring(ex.message()) + code;
          }
          applied_ = cmd.serial;
        }
        auto retryAfter = std::chrono::seconds{1};
        try {
          retryAfter = Read();
        } catch (winrt::hresult_error const &ex) {
          {
            std::lock_guard lock(mutex_);
            snapshot_.error =
                std::wstring(L"Cannot refresh audio devices: ") + ex.message().c_str();
            snapshot_.applied = applied_;
          }
          if (publish_)
            publish_();
        }
        std::unique_lock lock(wake_->mutex);
        auto signalled = [&] { return stop_ || wake_->changed; };
        // Failed reads and endpoint activation may have no notification to
        // wake the worker again. Retry only until the audio state recovers.
        if (retryAfter.count())
          wake_->cv.wait_for(lock, retryAfter, signalled);
        else
          wake_->cv.wait(lock, signalled);
        wake_->changed = false;
      }
      enumerator_->UnregisterEndpointNotificationCallback(observer.get());
      observer = nullptr;
    } catch (winrt::hresult_error const &ex) {
      {
        std::lock_guard lock(mutex_);
        snapshot_.error = std::wstring(L"Core Audio: ") + ex.message().c_str();
      }
      if (publish_)
        publish_();
    } catch (...) {
      {
        std::lock_guard lock(mutex_);
        snapshot_.error = L"Core Audio worker stopped unexpectedly";
      }
      if (publish_)
        publish_();
    }
    if (master_ && volumeObserver_)
      master_->UnregisterControlChangeNotify(volumeObserver_.get());
    volumeObserver_ = nullptr;
    master_ = nullptr;
    UnbindInputVolume();
    inputVolumeId_.clear();
    enumerator_ = nullptr;
    if (apartment)
      winrt::uninit_apartment();
  }

public:
  Audio() = default;
  Audio(Audio const &) = delete;
  Audio &operator=(Audio const &) = delete;
  ~Audio() { Stop(); }
  void Start(std::function<void()> publish, bool inputVolumeEnabled = false) {
    publish_ = std::move(publish);
    {
      std::lock_guard lock(volumeFeed_->mutex);
      volumeFeed_->publish = publish_;
      volumeFeed_->active = true;
    }
    stop_ = false;
    inputVolumeEnabled_ = inputVolumeEnabled;
    worker_ = std::thread([this] { Run(); });
  }
  void Stop() {
    {
      std::lock_guard lock(volumeFeed_->mutex);
      volumeFeed_->active = false;
      volumeFeed_->publish = {};
    }
    {
      // Under the wake mutex, so the worker cannot miss it between its check
      // and its wait.
      std::lock_guard lock(wake_->mutex);
      stop_ = true;
    }
    wake_->cv.notify_one();
    if (worker_.joinable())
      worker_.join();
    publish_ = {};
  }
  unsigned long long Send(Command command) {
    command.serial = ++next_;
    auto serial = command.serial;
    {
      std::lock_guard lock(mutex_);
      commands_.push_back(std::move(command));
    }
    wake_->Signal();
    return serial;
  }
  void EnableInputVolume(bool enabled) {
    inputVolumeEnabled_ = enabled;
    wake_->Signal();
  }
  Snapshot Get() {
    Snapshot state;
    {
      std::lock_guard lock(mutex_);
      state = snapshot_;
    }
    {
      std::lock_guard lock(volumeFeed_->mutex);
      if (volumeFeed_->available && volumeFeed_->id == state.outputId) {
        state.master = volumeFeed_->value;
        state.muted = volumeFeed_->muted;
        state.masterAvailable = true;
        state.masterRevision = volumeFeed_->revision;
        state.masterNoticeAt = volumeFeed_->noticeAt;
      }
    }
    return state;
  }
};
} // namespace sound

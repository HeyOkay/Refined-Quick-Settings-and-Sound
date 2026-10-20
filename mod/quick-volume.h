#pragma once
#include "common.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <winrt/Windows.System.h>
#include "model.h"
namespace native {
// System volume of the Quick Settings row, with the logic of SV on the Sound
// page: the slider is the mod's own and writes go straight to the default
// render endpoint (the role the Sound page uses), with the same mute rule
// (sound::SavedVolume: mute = level 0, unmute = last level). The shell's own
// slider updates the level through its pipeline and lags behind the pointer.
// Changes made elsewhere arrive through the endpoint's volume callback (the
// values come with it, no re-read); a default-device change re-opens the
// endpoint. Callbacks run on Core Audio threads and only queue work on the UI
// thread; Close() stops them.
class QuickVolume {
 struct State{std::mutex m;bool alive=true;winrt::Windows::System::DispatcherQueue queue{nullptr};std::function<void(float,bool)> changed;std::function<void()> reopen;};
 static void Post(std::shared_ptr<State> const& state,std::function<void(State&)> work){
  winrt::Windows::System::DispatcherQueue queue{nullptr};{std::lock_guard lock(state->m);if(!state->alive)return;queue=state->queue;}if(!queue)return;
  auto weak=std::weak_ptr<State>(state);queue.TryEnqueue([weak,work=std::move(work)]{auto s=weak.lock();if(!s)return;{std::lock_guard lock(s->m);if(!s->alive)return;}try{work(*s);}catch(...){}});
 }
 struct VolumeNotify:winrt::implements<VolumeNotify,IAudioEndpointVolumeCallback>{
  std::shared_ptr<State> state;explicit VolumeNotify(std::shared_ptr<State> s):state(std::move(s)){}
  HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA data)noexcept override{
   if(!data)return E_POINTER;float value=data->fMasterVolume;bool muted=data->bMuted;
   try{Post(state,[value,muted](State& s){if(s.changed)s.changed(value,muted);});}catch(...){}return S_OK;
  }
 };
 struct DeviceNotify:winrt::implements<DeviceNotify,IMMNotificationClient>{
  std::shared_ptr<State> state;explicit DeviceNotify(std::shared_ptr<State> s):state(std::move(s)){}
  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,ERole role,LPCWSTR)noexcept override{if(flow==eRender&&role==eMultimedia)try{Post(state,[](State& s){if(s.reopen)s.reopen();});}catch(...){}return S_OK;}
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR)noexcept override{return S_OK;}
  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR)noexcept override{return S_OK;}
  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD)noexcept override{return S_OK;}
  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY)noexcept override{return S_OK;}
 };
 std::shared_ptr<State> state_=std::make_shared<State>();
 winrt::com_ptr<IMMDeviceEnumerator> enumerator_;winrt::com_ptr<IAudioEndpointVolume> volume_;
 winrt::com_ptr<VolumeNotify> volumeNotify_;winrt::com_ptr<DeviceNotify> deviceNotify_;bool deviceRegistered_=false;
 std::map<std::wstring,sound::SavedVolume> saved_;std::wstring id_;int mutedSet_=-1;
 void CloseEndpoint(){if(volume_&&volumeNotify_)volume_->UnregisterControlChangeNotify(volumeNotify_.get());volume_=nullptr;id_.clear();mutedSet_=-1;}
 void OpenEndpoint(){
  CloseEndpoint();if(!enumerator_)return;winrt::com_ptr<IMMDevice> device;if(FAILED(enumerator_->GetDefaultAudioEndpoint(eRender,eMultimedia,device.put())))return;
  LPWSTR id=nullptr;if(SUCCEEDED(device->GetId(&id))&&id){id_=id;CoTaskMemFree(id);}
  if(FAILED(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_INPROC_SERVER,nullptr,volume_.put_void()))){volume_=nullptr;return;}
  if(volumeNotify_)volume_->RegisterControlChangeNotify(volumeNotify_.get());
 }
public:
 QuickVolume()=default;QuickVolume(QuickVolume const&)=delete;QuickVolume& operator=(QuickVolume const&)=delete;
 // On the UI thread. `changed(value,muted)` runs there after any change of the
 // endpoint's level or mute; `reopened` after the default device changed.
 bool Open(std::function<void(float,bool)> changed,std::function<void()> reopened){
  state_->queue=winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
  state_->changed=[this,changed=std::move(changed)](float value,bool muted){mutedSet_=muted?1:0;saved_[id_].Observe(value,muted);if(changed)changed(value,muted);};
  state_->reopen=[this,reopened=std::move(reopened)]{OpenEndpoint();if(reopened)reopened();};
  if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,__uuidof(IMMDeviceEnumerator),enumerator_.put_void())))return false;
  volumeNotify_=winrt::make_self<VolumeNotify>(state_);deviceNotify_=winrt::make_self<DeviceNotify>(state_);
  deviceRegistered_=SUCCEEDED(enumerator_->RegisterEndpointNotificationCallback(deviceNotify_.get()));OpenEndpoint();return bool(volume_);
 }
 void Close(){
  {std::lock_guard lock(state_->m);state_->alive=false;state_->changed={};state_->reopen={};}
  try{CloseEndpoint();}catch(...){}
  try{if(enumerator_&&deviceNotify_&&deviceRegistered_)enumerator_->UnregisterEndpointNotificationCallback(deviceNotify_.get());}catch(...){}
  deviceRegistered_=false;enumerator_=nullptr;
 }
 ~QuickVolume(){Close();}
 bool Read(float& value,bool& muted){
  if(!volume_)return false;BOOL m=FALSE;if(FAILED(volume_->GetMasterVolumeLevelScalar(&value))||FAILED(volume_->GetMute(&m)))return false;
  muted=m;mutedSet_=m?1:0;saved_[id_].Observe(value,muted);return true;
 }
 // As SV (audio.h Execute): level from the slider, muted exactly at 0. The
 // mute is written only when it changes, so a drag costs one call per step.
 void SetValue(float value){
  if(!volume_)return;value=sound::Clamp(value);volume_->SetMasterVolumeLevelScalar(value,nullptr);
  int muted=value<=0?1:0;if(muted!=mutedSet_){volume_->SetMute(muted,nullptr);mutedSet_=muted;}
  saved_[id_].Observe(value,muted);
 }
 void ToggleMute(){
  if(!volume_)return;float before=0;BOOL muted=FALSE;if(FAILED(volume_->GetMasterVolumeLevelScalar(&before))||FAILED(volume_->GetMute(&muted)))return;
  auto& saved=saved_[id_];saved.Observe(before,muted);auto value=saved.Toggle(before,muted);
  volume_->SetMasterVolumeLevelScalar(value,nullptr);volume_->SetMute(value<=0,nullptr);mutedSet_=value<=0?1:0;
 }
};
}

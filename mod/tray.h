#pragma once
#include "volume-icon.h"
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
namespace native::tray {
inline std::atomic<bool> stopping{false},moduleHooked{false};

inline sound::Audio audio;
inline HINSTANCE moduleInstance=nullptr;
struct Job{std::function<void()> call;};
inline std::mutex jobsMutex,recordsMutex;
inline std::map<UINT_PTR,std::shared_ptr<Job>> jobs;
inline std::atomic<UINT_PTR> nextJob{0};
inline UINT dispatchMessage=RegisterWindowMessageW(L"Windhawk.SoundPanel.ThreadDispatch.1");
inline LRESULT CALLBACK DispatchHook(int code,WPARAM w,LPARAM l) {
    if(code>=0){auto message=reinterpret_cast<CWPSTRUCT*>(l);if(message->message==dispatchMessage){
        std::shared_ptr<Job> job;{std::lock_guard lock(jobsMutex);auto it=jobs.find(message->wParam);if(it!=jobs.end())job=it->second;}
        if(job)try{job->call();}catch(winrt::hresult_error const& ex){Wh_Log(L"Tray UI error 0x%08X",ex.code().value);}
    }}return CallNextHookEx(nullptr,code,w,l);
}
inline void Dispatch(HWND target,std::function<void()> action) {
    DWORD thread=GetWindowThreadProcessId(target,nullptr);if(!thread)return;
    if(thread==GetCurrentThreadId()){action();return;}
    auto hook=SetWindowsHookExW(WH_CALLWNDPROC,DispatchHook,moduleInstance,thread);if(!hook){Wh_Log(L"Tray dispatch hook failed: %u",GetLastError());return;}
    auto job=std::make_shared<Job>();job->call=std::move(action);auto cookie=++nextJob;
    {std::lock_guard lock(jobsMutex);jobs.emplace(cookie,job);}
    // Synchronous dispatch is deliberate: no DLL callback may outlive unload.
    SendMessageW(target,dispatchMessage,cookie,0);
    {std::lock_guard lock(jobsMutex);jobs.erase(cookie);}UnhookWindowsHookEx(hook);
}
inline std::vector<HWND> Taskbars() {
    std::vector<HWND> result;EnumWindows([](HWND window,LPARAM data)->BOOL{
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId())return TRUE;
        WCHAR name[64]{};GetClassNameW(window,name,64);
        if(!_wcsicmp(name,L"Shell_TrayWnd")||!_wcsicmp(name,L"Shell_SecondaryTrayWnd"))reinterpret_cast<std::vector<HWND>*>(data)->push_back(window);return TRUE;
    },reinterpret_cast<LPARAM>(&result));return result;
}
inline void* siteVtable=nullptr;
using GetHost=void*(WINAPI*)(void*,void**);inline GetHost getHost=nullptr;
using Decref=void(WINAPI*)(void*);inline Decref decref=nullptr;
inline void* frameHeight=nullptr;
inline bool Readable(void const* pointer,size_t bytes) {
    MEMORY_BASIC_INFORMATION info{};if(!pointer||!VirtualQuery(pointer,&info,sizeof(info))||info.State!=MEM_COMMIT||info.Protect&(PAGE_GUARD|PAGE_NOACCESS))return false;
    auto address=reinterpret_cast<uintptr_t>(pointer),end=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;return bytes<=end-address;
}
inline X::XamlRoot Root(HWND taskbar) {
    if(!getHost||!siteVtable||!decref||!frameHeight)return nullptr;
    auto bandWindow=reinterpret_cast<HWND>(GetPropW(taskbar,L"TaskbandHWND"));if(!bandWindow)return nullptr;
    auto band=reinterpret_cast<void**>(GetWindowLongPtrW(bandWindow,0));void** site=nullptr;
    for(int i=0;i<20;i++){if(!Readable(band+i,sizeof(void*)))break;if(band[i]==siteVtable){site=band+i;break;}}
    if(!site)return nullptr;
    auto code=static_cast<BYTE const*>(frameHeight);
    if(!Readable(code,8)||code[0]!=0x48||code[1]!=0x83||code[2]!=0xec||code[4]!=0x48||code[5]!=0x83||code[6]!=0xc1||code[7]>0x7f){Wh_Log(L"Unsupported TaskbarHost layout; native icon retained");return nullptr;}
    void* shared[2]{};getHost(site,shared);X::XamlRoot root{nullptr};
    if(shared[0]){auto address=reinterpret_cast<IUnknown**>(static_cast<BYTE*>(shared[0])+code[7]);
        if(Readable(address,sizeof(void*))&&*address)try{X::FrameworkElement value{nullptr};(*address)->QueryInterface(winrt::guid_of<X::FrameworkElement>(),winrt::put_abi(value));if(value)root=value.XamlRoot();}catch(...){}
    }if(shared[1])decref(shared[1]);return root;
}
struct Record {
    DWORD thread=0;HWND taskbar=nullptr;
    winrt::weak_ref<X::FrameworkElement> icon,native;
    winrt::weak_ref<C::Panel> parent;
    X::FrameworkElement visual{nullptr};X::DispatcherTimer timer{nullptr};
    winrt::event_token loaded{},tick{};
    Journal glyphJournal;std::shared_ptr<AnimatedVolume> animated;
    std::vector<std::function<void()>> glyphRevoke;
    int displayed=-1,hiddenCount=-1;unsigned attempts=0;bool attached=false;
    int kind=0,netState=0;bool networkSeen=false;// kind: 1 volume, 2 wired network
};
inline std::vector<std::shared_ptr<Record>> records;
inline bool VolumeLeaf(X::DependencyObject const& node) {
    winrt::hstring value;if(auto text=node.try_as<C::TextBlock>())value=text.Text();else if(auto icon=node.try_as<C::FontIcon>())value=icon.Glyph();
    if(value.size()!=1)return false;switch(value[0]){case 0xe74f:case 0xe992:case 0xe993:case 0xe994:case 0xe995:case 0xea85:case 0xebc5:return true;}return false;
}
// Wired network icon of the same ControlCenterButton group. Windows draws it
// with Segoe Fluent Icons E839 (connected) and F8CC (the same glyph with a
// cut-out corner for a badge); for no internet or no connection it shows the
// globe F384, for any network type. The mod shows Lucide ethernet-port with
// matching badges: the globe becomes ethernet-port with the cross badge only
// while a wired hardware adapter has its cable connected, otherwise it stays
// native. Wi-Fi and every other glyph stay native.
inline wchar_t LeafCode(X::DependencyObject const& node){winrt::hstring value;if(auto text=node.try_as<C::TextBlock>())value=text.Text();else if(auto icon=node.try_as<C::FontIcon>())value=icon.Glyph();return value.size()==1?value[0]:0;}
inline int NetworkLeaf(X::DependencyObject const& node){switch(LeafCode(node)){case 0xe839:case 0xf8cc:return 1;case 0xeb55:return 2;case 0xeb56:return 3;case 0xf384:return 4;}return 0;}
// A wired hardware adapter with its medium connected (not Hyper-V/WSL/VPN virtual ones).
inline bool WiredConnected(){PMIB_IF_TABLE2 table=nullptr;if(GetIfTable2(&table)!=NO_ERROR||!table)return false;bool found=false;
    for(ULONG i=0;i<table->NumEntries&&!found;i++){auto const& row=table->Table[i];found=row.Type==IF_TYPE_ETHERNET_CSMACD&&row.InterfaceAndOperStatusFlags.HardwareInterface&&!row.InterfaceAndOperStatusFlags.FilterInterface&&row.MediaConnectState==MediaConnectStateConnected;}
    FreeMibTable(table);return found;}
inline M::Brush LeafBrush(X::FrameworkElement const& leaf){if(auto text=leaf.try_as<C::TextBlock>())return text.Foreground();if(auto icon=leaf.try_as<C::IconElement>())return icon.Foreground();return nullptr;}
inline double LeafSize(X::FrameworkElement const& leaf){double size=0;if(auto text=leaf.try_as<C::TextBlock>())size=text.FontSize();else if(auto icon=leaf.try_as<C::FontIcon>())size=icon.FontSize();return size>0&&size<64?size:16;}
inline X::FrameworkElement NetworkVisual(int state,double size){
    PCWSTR outline=state==2?sound::ethernetPortCutX:state==3?sound::ethernetPortCutWarn:sound::ethernetPort;
    PCWSTR badge=state==2?sound::ethernetBadgeX:state==3?sound::ethernetBadgeWarn:nullptr;
    std::wstring markup=L"<Viewbox xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' Width='"+std::to_wstring(size)+L"' Height='"+std::to_wstring(size)+L"' HorizontalAlignment='Center' VerticalAlignment='Center' IsHitTestVisible='False'><Canvas Width='24' Height='24'><Path StrokeThickness='1.5' StrokeStartLineCap='Round' StrokeEndLineCap='Round' StrokeLineJoin='Round' Data='"+std::wstring(outline)+L"'/>";
    if(badge)markup+=L"<Path Data='"+std::wstring(badge)+L"'/>";
    markup+=L"</Canvas></Viewbox>";
    return X::Markup::XamlReader::Load(markup).as<X::FrameworkElement>();
}
// The glyph's own brush (theme, high contrast), copied on every pass.
inline void PaintNetwork(X::FrameworkElement const& visual,M::Brush const& brush){
    auto box=visual.try_as<C::Viewbox>();if(!box||!brush)return;auto canvas=box.Child().try_as<C::Canvas>();if(!canvas)return;auto children=canvas.Children();
    if(children.Size()>0)if(auto path=children.GetAt(0).try_as<X::Shapes::Path>();path&&path.Stroke()!=brush)path.Stroke(brush);
    if(children.Size()>1)if(auto path=children.GetAt(1).try_as<X::Shapes::Path>();path&&path.Fill()!=brush)path.Fill(brush);
}
template<class F>inline X::FrameworkElement Find(X::DependencyObject const& node,F const& predicate) {
    if(auto element=node.try_as<X::FrameworkElement>();element&&predicate(element))return element;
    for(int i=0;i<M::VisualTreeHelper::GetChildrenCount(node);i++)if(auto match=Find(M::VisualTreeHelper::GetChild(node,i),predicate))return match;return nullptr;
}
inline bool InControlCenter(X::FrameworkElement element) {
    while(element){if(element.Name()==L"ControlCenterButton")return true;element=M::VisualTreeHelper::GetParent(element).try_as<X::FrameworkElement>();}return false;
}
inline void CopySlot(X::FrameworkElement const& to,X::FrameworkElement const& from) {
    to.HorizontalAlignment(from.HorizontalAlignment());to.VerticalAlignment(from.VerticalAlignment());to.Margin(from.Margin());
    C::Grid::SetColumn(to,C::Grid::GetColumn(from));C::Grid::SetRow(to,C::Grid::GetRow(from));C::Grid::SetColumnSpan(to,C::Grid::GetColumnSpan(from));C::Grid::SetRowSpan(to,C::Grid::GetRowSpan(from));C::Canvas::SetZIndex(to,C::Canvas::GetZIndex(from));to.IsHitTestVisible(false);
}
inline void RestoreGlyphs(std::shared_ptr<Record> const& record){for(auto& revoke:record->glyphRevoke)try{revoke();}catch(...){Wh_Log(L"Glyph callback removal: %08X",winrt::to_hresult());}record->glyphRevoke.clear();record->glyphJournal.Restore();}
inline void RemoveVisual(std::shared_ptr<Record> const& record){if(auto parent=record->parent.get();parent&&record->visual){uint32_t index;if(parent.Children().IndexOf(record->visual,index))parent.Children().RemoveAt(index);}record->visual=nullptr;}
// Opacity only, journaled, kept at 0 against template/transition writes (as volume).
inline void HideLeaves(std::shared_ptr<Record> const& record,std::vector<X::FrameworkElement> const& leaves){
    for(auto const& node:leaves){
        auto property=X::UIElement::OpacityProperty();auto key=std::make_pair(winrt::get_abi(node),winrt::get_abi(property));
        bool fresh=!record->glyphJournal.saved.contains(key);record->glyphJournal.Save(node,property);if(node.Opacity()!=0)node.Opacity(0);
        if(fresh)try{auto guard=std::make_shared<bool>(false);auto weak=winrt::make_weak(node);
            auto token=node.RegisterPropertyChangedCallback(property,[weak,guard](auto&&,auto&&){if(stopping||*guard)return;if(auto glyph=weak.get();glyph&&glyph.Opacity()!=0){*guard=true;try{glyph.Opacity(0);}catch(...){}*guard=false;}});
            record->glyphRevoke.push_back([node,property,token]{node.UnregisterPropertyChangedCallback(property,token);});
        }catch(...){Wh_Log(L"Network glyph opacity watch unavailable; timer retained: %08X",winrt::to_hresult());}
    }
}
inline void RefreshNetwork(std::shared_ptr<Record> const& record,X::FrameworkElement const& icon){
    auto parent=record->parent.get();if(!parent)return;
    std::vector<X::FrameworkElement> leaves;int state=0;
    Find(icon,[&](auto const& node){if(int s=NetworkLeaf(node)){leaves.push_back(node);if(!state)state=s;}return false;});
    bool globeWithoutCable=state==4&&!WiredConnected();
    if(!state||globeWithoutCable){
        // The icon shows something else now (Wi-Fi, globe without cable): native
        // again; the next passes look for a supported glyph from scratch.
        RemoveVisual(record);RestoreGlyphs(record);record->attached=false;record->kind=0;record->netState=0;record->native=nullptr;record->parent=nullptr;record->attempts=0;
        return;
    }
    auto brush=LeafBrush(leaves.front());
    if(state!=record->netState||!record->visual){
        // The new picture is attached before the old one goes and before any
        // native glyph is hidden: a failure leaves the native icon visible.
        auto visual=NetworkVisual(state==4?2:state,LeafSize(leaves.front()));visual.Name(L"NativeSoundTrayNetwork");
        if(auto grid=parent.try_as<C::Grid>()){C::Grid::SetColumnSpan(visual,std::max(1u,grid.ColumnDefinitions().Size()));C::Grid::SetRowSpan(visual,std::max(1u,grid.RowDefinitions().Size()));}
        PaintNetwork(visual,brush);parent.Children().Append(visual);RemoveVisual(record);record->visual=visual;record->netState=state;
    }else PaintNetwork(record->visual,brush);
    HideLeaves(record,leaves);
}
inline void Refresh(std::shared_ptr<Record> const& record) {
    if(stopping)return;auto icon=record->icon.get();if(!icon){if(record->timer)record->timer.Stop();return;}
    if(record->attached){auto native=record->native.get();if(!native||!native.IsLoaded()){
        if(auto parent=record->parent.get();parent&&record->visual){uint32_t index;if(parent.Children().IndexOf(record->visual,index))parent.Children().RemoveAt(index);}
        RestoreGlyphs(record);record->visual=nullptr;record->animated.reset();record->native=nullptr;record->parent=nullptr;record->attached=false;record->displayed=-1;record->kind=0;record->netState=0;
    }}
    if(!record->attached) {
        if(!InControlCenter(icon)){if(record->timer)record->timer.Stop();return;}
        // Use the outermost Grid within this IconView. The replacement must
        // remain outside every native glyph/template branch that we suppress.
        int kind=1;auto leaf=Find(icon,[](auto const& node){return VolumeLeaf(node);});
        if(!leaf){int wired=-1;leaf=Find(icon,[&wired](auto const& node){int s=NetworkLeaf(node);if(s==4){if(wired<0)wired=WiredConnected();return wired==1;}return s!=0;});kind=2;}
        X::FrameworkElement glyph{nullptr};C::Grid parent{nullptr};
        for(auto slot=leaf;slot&&slot!=icon;slot=M::VisualTreeHelper::GetParent(slot).try_as<X::FrameworkElement>()){
            auto owner=M::VisualTreeHelper::GetParent(slot);
            if(auto grid=owner.try_as<C::Grid>()){glyph=slot;parent=grid;}
        }
        if(!glyph){
            // Icons without a supported glyph keep a slow poll (2 s): Windows
            // switches the same icon between Wi-Fi, the globe and Ethernet.
            ++record->attempts;
            if(record->attempts>=8&&record->timer&&record->timer.Interval()!=winrt::Windows::Foundation::TimeSpan(std::chrono::seconds(2)))record->timer.Interval(std::chrono::seconds(2));return;
        }
        record->native=winrt::make_weak(glyph);record->parent=winrt::make_weak(parent.as<C::Panel>());
        record->attached=true;record->kind=kind;if(kind==2)record->networkSeen=true;if(record->timer)record->timer.Interval(std::chrono::milliseconds(300));
    }
    if(record->kind==2){RefreshNetwork(record,icon);return;}
    auto state=audio.Get();
    auto native=record->native.get();auto parent=record->parent.get();if(!native||!parent)return;
    if(!state.masterAvailable){
        if(record->visual){uint32_t index;if(parent.Children().IndexOf(record->visual,index))parent.Children().RemoveAt(index);}
        RestoreGlyphs(record);record->visual=nullptr;record->animated.reset();return;
    }
    if(!record->animated){
        // Construct and attach the real system AnimatedIcon before suppressing
        // any native leaf. Failure leaves the original taskbar icon visible.
        auto animated=std::make_shared<AnimatedVolume>();auto replacement=animated->Element();replacement.Name(L"NativeSoundTrayGlyph");
        if(auto grid=parent.try_as<C::Grid>()){C::Grid::SetColumnSpan(replacement,std::max(1u,grid.ColumnDefinitions().Size()));C::Grid::SetRowSpan(replacement,std::max(1u,grid.RowDefinitions().Size()));}
        parent.Children().Append(replacement);record->visual=replacement;record->animated=std::move(animated);
    }
    record->animated->Update(state.master,state.muted);
    // Never hide the old image before its replacement is attached. No clipping
    // or layout changes: only glyph-leaf opacity, including transition copies.
    try{
        int hidden=0;std::vector<X::FrameworkElement> leaves;
        Find(icon,[&](auto const& node){if(VolumeLeaf(node))leaves.push_back(node);return false;});
        for(auto const& node:leaves){
            auto property=X::UIElement::OpacityProperty();auto key=std::make_pair(winrt::get_abi(node),winrt::get_abi(property));
            bool fresh=!record->glyphJournal.saved.contains(key);record->glyphJournal.Save(node,property);node.Opacity(0);hidden++;
            if(fresh)try{auto guard=std::make_shared<bool>(false);auto weak=winrt::make_weak(node);
                auto token=node.RegisterPropertyChangedCallback(property,[weak,guard](auto&&,auto&&){if(stopping||*guard)return;if(auto glyph=weak.get();glyph&&glyph.Opacity()!=0){*guard=true;try{glyph.Opacity(0);}catch(...){}*guard=false;}});
                record->glyphRevoke.push_back([node,property,token]{node.UnregisterPropertyChangedCallback(property,token);});
            }catch(...){Wh_Log(L"Native glyph opacity watch unavailable; timer retained: %08X",winrt::to_hresult());}
        }
        if(hidden!=record->hiddenCount){record->hiddenCount=hidden;}
    }catch(...){
        Wh_Log(L"TrayLastError %08X",winrt::to_hresult());RestoreGlyphs(record);
        if(record->visual){uint32_t index;if(parent.Children().IndexOf(record->visual,index))parent.Children().RemoveAt(index);record->visual=nullptr;}record->displayed=-1;throw;
    }
}
inline std::mutex pulsesMutex;inline std::map<HWND,std::shared_ptr<VolumePulse>> pulses;
inline LRESULT CALLBACK VolumeProc(HWND window,UINT message,WPARAM w,LPARAM l,DWORD_PTR){
    if(message==volumePulseMessage){
        {std::lock_guard lock(pulsesMutex);if(auto it=pulses.find(window);it!=pulses.end())it->second->queued=false;}
        if(!stopping){std::vector<std::shared_ptr<Record>> local;{std::lock_guard lock(recordsMutex);for(auto const& record:records)if(record->taskbar==window&&record->attached)local.push_back(record);}
            for(auto const& record:local)try{Refresh(record);}catch(...){Wh_Log(L"TrayAnimationError %08X",winrt::to_hresult());}}
        return 0;
    }
    if(message==WM_NCDESTROY){{std::lock_guard lock(pulsesMutex);if(auto it=pulses.find(window);it!=pulses.end()){it->second->active=false;pulses.erase(it);}}WindhawkUtils::RemoveWindowSubclassFromAnyThread(window,VolumeProc);}
    return DefSubclassProc(window,message,w,l);
}
inline void EnsurePulse(HWND window){
    {std::lock_guard lock(pulsesMutex);if(pulses.contains(window))return;}
    if(WindhawkUtils::SetWindowSubclassFromAnyThread(window,VolumeProc,0)){std::lock_guard lock(pulsesMutex);pulses.emplace(window,std::make_shared<VolumePulse>(window));}
}
inline void VolumeChanged(){std::vector<std::shared_ptr<VolumePulse>> copy;{std::lock_guard lock(pulsesMutex);for(auto const& [window,pulse]:pulses)copy.push_back(pulse);}for(auto const& pulse:copy)pulse->Post();}
inline void Observe(X::FrameworkElement const& element,HWND taskbar) {
    if(!element||stopping)return;EnsurePulse(taskbar);
    {std::lock_guard lock(recordsMutex);for(auto const& r:records)if(auto current=r->icon.get();current&&current.as<IUnknown>()==element.as<IUnknown>())return;}
    auto record=std::make_shared<Record>();record->thread=GetCurrentThreadId();record->taskbar=taskbar;record->icon=winrt::make_weak(element);
    std::weak_ptr<Record> weak=record;record->loaded=element.Loaded([weak](auto&&,auto&&){if(auto r=weak.lock())try{r->attempts=0;Refresh(r);if(r->attached||InControlCenter(r->icon.get()))r->timer.Start();}catch(...){Wh_Log(L"Volume icon attach failed");}});
    record->timer=X::DispatcherTimer();record->timer.Interval(std::chrono::milliseconds(300));record->tick=record->timer.Tick([weak](auto&&,auto&&){if(auto r=weak.lock())try{Refresh(r);}catch(...){Wh_Log(L"TrayAnimationError %08X",winrt::to_hresult());r->timer.Stop();Wh_Log(L"Volume icon refresh failed");}});
    {std::lock_guard lock(recordsMutex);records.push_back(record);}Refresh(record);if(record->attached||InControlCenter(element))record->timer.Start();
}
inline void Walk(X::DependencyObject const& node,HWND taskbar) {
    if(auto element=node.try_as<X::FrameworkElement>())if(VolumeLeaf(element)||NetworkLeaf(element)) {
        auto icon=element;for(auto ancestor=element;ancestor;ancestor=M::VisualTreeHelper::GetParent(ancestor).try_as<X::FrameworkElement>()){
            if(std::wstring_view(winrt::get_class_name(ancestor)).find(L"SystemTray.IconView")!=std::wstring_view::npos){icon=ancestor;break;}}
        if(InControlCenter(icon))Observe(icon,taskbar);return;
    }
    for(int i=0;i<M::VisualTreeHelper::GetChildrenCount(node);i++)Walk(M::VisualTreeHelper::GetChild(node,i),taskbar);
}
using Construct=void*(WINAPI*)(void*);inline Construct originalConstructor=nullptr;
inline void* WINAPI Constructed(void* self) {
    auto result=originalConstructor(self);if(stopping)return result;
    // C++/WinRT implementation's projected interface occupies the second slot.
    try{auto address=reinterpret_cast<IUnknown**>(self)+1;if(Readable(address,sizeof(void*))&&*address){X::FrameworkElement element{nullptr};(*address)->QueryInterface(winrt::guid_of<X::FrameworkElement>(),winrt::put_abi(element));
        if(element){HWND taskbar=nullptr;for(auto window:Taskbars())if(GetWindowThreadProcessId(window,nullptr)==GetCurrentThreadId()){taskbar=window;break;}if(taskbar)Observe(element,taskbar);}
    }}catch(...){Wh_Log(L"New tray icon observation failed");}return result;
}
inline void OpenNativeSound(){
    INPUT input[6]{};UINT count=0;bool winHeld=(GetAsyncKeyState(VK_LWIN)&0x8000)!=0,ctrlHeld=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
    auto key=[&](WORD code,bool up){input[count].type=INPUT_KEYBOARD;input[count].ki.wVk=code;input[count++].ki.dwFlags=up?KEYEVENTF_KEYUP:0;};
    if(!winHeld)key(VK_LWIN,false);if(!ctrlHeld)key(VK_CONTROL,false);key('V',false);key('V',true);if(!ctrlHeld)key(VK_CONTROL,true);if(!winHeld)key(VK_LWIN,true);
    auto sent=SendInput(count,input,sizeof(INPUT));Wh_Log(L"Native Sound shortcut: %u/%u events",sent,count);
}
using NativeTapped=void(WINAPI*)(void*,IInspectable const&,X::Input::TappedRoutedEventArgs);
inline NativeTapped originalTapped=nullptr;
inline void WINAPI Tapped(void* self,IInspectable const& sender,X::Input::TappedRoutedEventArgs args) {
    if(!stopping)try{
        // Intercept the shell's own handler, before it activates Quick Settings.
        // A later UIElement.Tapped subscription cannot cancel an action already
        // performed by this handler.
        auto address=reinterpret_cast<IUnknown**>(self)+1;
        if(Readable(address,sizeof(void*))&&*address){X::FrameworkElement icon{nullptr};
            (*address)->QueryInterface(winrt::guid_of<X::FrameworkElement>(),winrt::put_abi(icon));
            if(icon&&InControlCenter(icon)&&Find(icon,[](auto const& child){return VolumeLeaf(child);})){
                args.Handled(true);
                Wh_Log(L"Sound panel: native volume click intercepted");
                OpenNativeSound();
                try{for(auto taskbar:Taskbars())if(GetWindowThreadProcessId(taskbar,nullptr)==GetCurrentThreadId()){Observe(icon,taskbar);break;}}
                catch(...){Wh_Log(L"Lazy volume icon styling failed; click was still handled");}
                return;
            }
        }
    }catch(winrt::hresult_error const& error){Wh_Log(L"Native volume handler: 0x%08X",error.code().value);}
    originalTapped(self,sender,std::move(args));
}
inline bool HookTray(HMODULE module) {
    // Exact decorated symbols verified in the installed SystemTray.pdb.
    WindhawkUtils::SYMBOL_HOOK hooks[]={
        {{L"??0IconView@implementation@SystemTray@winrt@@QEAA@XZ"},&originalConstructor,Constructed},
        {{L"?OnTappedHandler@IconView@implementation@SystemTray@winrt@@AEAAXAEBUIInspectable@Foundation@Windows@4@UTappedRoutedEventArgs@Input@Xaml@UI@74@@Z"},&originalTapped,Tapped}
    };
    WH_HOOK_SYMBOLS_OPTIONS options{};options.optionsSize=sizeof(options);options.noUndecoratedSymbols=TRUE;
    auto result=WindhawkUtils::HookSymbols(module,hooks,ARRAYSIZE(hooks),&options);
    return result;
}
using Load=decltype(&LoadLibraryExW);inline Load originalLoad=nullptr;
inline HMODULE WINAPI Loaded(LPCWSTR path,HANDLE file,DWORD flags) {
    auto module=originalLoad(path,file,flags);if(module&&!stopping&&module==GetModuleHandleW(L"SystemTray.dll")&&!moduleHooked.exchange(true))if(HookTray(module))Wh_ApplyHookOperations();return module;
}
inline bool Initialize(HINSTANCE instance) {
    moduleInstance=instance;stopping=false;
    auto taskbar=GetModuleHandleW(L"Taskbar.dll");
    if(!taskbar)taskbar=LoadLibraryExW(L"taskbar.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!taskbar){Wh_Log(L"Taskbar.dll unavailable: %u",GetLastError());return false;}
    WindhawkUtils::SYMBOL_HOOK roots[]={
        {{LR"(const CTaskBand::`vftable'{for `ITaskListWndSite'})"},&siteVtable},
        {{LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CTaskBand::GetTaskbarHost(void)const )"},&getHost},
        {{LR"(public: int __cdecl TaskbarHost::FrameHeight(void)const )"},&frameHeight},
        {{LR"(public: void __cdecl std::_Ref_count_base::_Decref(void))"},&decref}
    };
    if(!WindhawkUtils::HookSymbols(taskbar,roots,ARRAYSIZE(roots)))Wh_Log(L"Taskbar root symbols unavailable; constructor/tap discovery retained");
    if(auto module=GetModuleHandleW(L"SystemTray.dll")){moduleHooked=true;auto result=HookTray(module);if(result)audio.Start(VolumeChanged);return result;}
    auto load=reinterpret_cast<Load>(GetProcAddress(GetModuleHandleW(L"kernelbase.dll"),"LoadLibraryExW"));auto result=WindhawkUtils::SetFunctionHook(load,Loaded,&originalLoad);if(result)audio.Start(VolumeChanged);return result;
}
inline void AttachExisting() {
    for(auto taskbar:Taskbars())Dispatch(taskbar,[taskbar]{EnsurePulse(taskbar);auto root=Root(taskbar);if(root)Walk(root.Content(),taskbar);});
    size_t attached=0;{std::lock_guard lock(recordsMutex);for(auto const& r:records)if(r->attached)attached++;}
    Wh_Log(L"Sound panel: attached %zu volume icons",attached);
}
inline void Shutdown() {
    stopping=true;{std::lock_guard lock(pulsesMutex);for(auto const& [window,pulse]:pulses)pulse->active=false;}audio.Stop();
    std::vector<HWND> windows=Taskbars();{std::lock_guard lock(recordsMutex);for(auto const& r:records)if(r->taskbar&&std::find(windows.begin(),windows.end(),r->taskbar)==windows.end())windows.push_back(r->taskbar);}
    for(auto window:windows)if(IsWindow(window))Dispatch(window,[window]{
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(window,VolumeProc);
        std::vector<std::shared_ptr<Record>> local;
        {std::lock_guard lock(recordsMutex);for(auto it=records.begin();it!=records.end();)if((*it)->thread==GetCurrentThreadId()){local.push_back(*it);it=records.erase(it);}else ++it;}
        for(auto const& r:local){if(r->timer){r->timer.Stop();r->timer.Tick(r->tick);}if(auto icon=r->icon.get()){icon.Loaded(r->loaded);}
            RestoreGlyphs(r);
            if(auto parent=r->parent.get())if(r->visual){uint32_t index;if(parent.Children().IndexOf(r->visual,index))parent.Children().RemoveAt(index);}
        }
    });
    {std::lock_guard lock(pulsesMutex);pulses.clear();}
}
}

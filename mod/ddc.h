#pragma once
#include <windows.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
#include <algorithm>
#include <cmath>
// Monitor brightness over DDC/CI (dxva2, loaded at run time) for displays
// without a native brightness slider. All monitor I/O runs on one worker
// thread: a DDC/CI request takes tens of milliseconds and must not block the
// shell's UI thread. Writes are coalesced to the latest value and spaced at
// least 50 ms apart, which monitors tolerate.
namespace native {
struct PhysicalMonitor{HANDLE handle;WCHAR description[128];};
class Ddc {
 using CountFn=BOOL(WINAPI*)(HMONITOR,DWORD*);using GetFn=BOOL(WINAPI*)(HMONITOR,DWORD,PhysicalMonitor*);using DestroyFn=BOOL(WINAPI*)(DWORD,PhysicalMonitor*);
 using ReadFn=BOOL(WINAPI*)(HANDLE,DWORD*,DWORD*,DWORD*);using WriteFn=BOOL(WINAPI*)(HANDLE,DWORD);
 struct Api{CountFn count=nullptr;GetFn get=nullptr;DestroyFn destroy=nullptr;ReadFn read=nullptr;WriteFn write=nullptr;bool ok=false;};
 static Api const& Functions(){static Api api=[]{Api a;auto dll=LoadLibraryExW(L"dxva2.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll)return a;
  a.count=reinterpret_cast<CountFn>(GetProcAddress(dll,"GetNumberOfPhysicalMonitorsFromHMONITOR"));a.get=reinterpret_cast<GetFn>(GetProcAddress(dll,"GetPhysicalMonitorsFromHMONITOR"));
  a.destroy=reinterpret_cast<DestroyFn>(GetProcAddress(dll,"DestroyPhysicalMonitors"));a.read=reinterpret_cast<ReadFn>(GetProcAddress(dll,"GetMonitorBrightness"));a.write=reinterpret_cast<WriteFn>(GetProcAddress(dll,"SetMonitorBrightness"));
  a.ok=a.count&&a.get&&a.destroy&&a.read&&a.write;return a;}();return api;}
 struct Range{DWORD low=0,high=100;bool known=false;};
 std::mutex lock_;std::condition_variable wake_;std::thread worker_;bool stop_=false;
 HMONITOR readMonitor_=nullptr,writeMonitor_=nullptr;int want_=-1;std::map<HMONITOR,Range> ranges_;
 // Results for the UI thread.
 HMONITOR resultMonitor_=nullptr;bool resultSupported_=false;int resultValue_=-1;unsigned revision_=0;int lastError_=0;
 template<class F>static bool Each(HMONITOR monitor,F&& f){
  auto& api=Functions();if(!api.ok||!monitor)return false;DWORD n=0;if(!api.count(monitor,&n)||!n)return false;
  std::vector<PhysicalMonitor> list(n);if(!api.get(monitor,n,list.data()))return false;bool any=false;for(auto& m:list)any|=f(m.handle);api.destroy(n,list.data());return any;
 }
 void Run(){
  for(;;){
   HMONITOR read=nullptr,write=nullptr;int value=-1;
   {std::unique_lock guard(lock_);wake_.wait(guard,[&]{return stop_||readMonitor_||want_>=0;});if(stop_)return;
    read=readMonitor_;readMonitor_=nullptr;write=writeMonitor_;value=want_;want_=-1;}
   if(write&&value>=0){
    Range range;{std::lock_guard guard(lock_);range=ranges_[write];}
    if(!range.known)Each(write,[&](HANDLE h){DWORD lo=0,cur=0,hi=0;if(!Functions().read(h,&lo,&cur,&hi)||hi<=lo)return false;range={lo,hi,true};return true;});
    bool ok=range.known&&Each(write,[&](HANDLE h){return bool(Functions().write(h,range.low+DWORD(std::lround(value*(range.high-range.low)/100.))));});
    {std::lock_guard guard(lock_);if(range.known)ranges_[write]=range;if(!ok)lastError_=int(GetLastError());}
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }
   if(read){
    Range range;int current=-1;
    bool ok=Each(read,[&](HANDLE h){DWORD lo=0,cur=0,hi=0;if(!Functions().read(h,&lo,&cur,&hi)||hi<=lo)return false;if(current<0){range={lo,hi,true};current=int(std::lround((cur-lo)*100./(hi-lo)));}return true;});
    std::lock_guard guard(lock_);if(ok)ranges_[read]=range;else lastError_=int(GetLastError());
    resultMonitor_=read;resultSupported_=ok;resultValue_=current;revision_++;
   }
  }
 }
public:
 Ddc(){worker_=std::thread([this]{Run();});}
 Ddc(Ddc const&)=delete;Ddc& operator=(Ddc const&)=delete;
 ~Ddc(){{std::lock_guard guard(lock_);stop_=true;}wake_.notify_all();if(worker_.joinable())worker_.join();}
 void Read(HMONITOR monitor){{std::lock_guard guard(lock_);readMonitor_=monitor;}wake_.notify_all();}
 void Write(HMONITOR monitor,int value){{std::lock_guard guard(lock_);writeMonitor_=monitor;want_=std::clamp(value,0,100);}wake_.notify_all();}
 struct Result{HMONITOR monitor;bool supported;int value;unsigned revision;int error;};
 Result Latest(){std::lock_guard guard(lock_);return {resultMonitor_,resultSupported_,resultValue_,revision_,lastError_};}
};
}

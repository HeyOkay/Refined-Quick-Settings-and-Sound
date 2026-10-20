#pragma once
#include "bridge.h"
namespace native {
using OpenKey=decltype(&RegOpenKeyExW);inline OpenKey originalOpenKey=nullptr;
using QueryValue=decltype(&RegQueryValueExW);inline QueryValue originalQueryValue=nullptr;
inline LSTATUS WINAPI OpenKeyHook(HKEY key,LPCWSTR sub,DWORD options,REGSAM access,PHKEY result){auto status=originalOpenKey(key,sub,options,access,result);if(status!=ERROR_SUCCESS&&compositionDisabled&&key==HKEY_LOCAL_MACHINE&&sub&&!_wcsicmp(sub,L"Software\\Microsoft\\XAML\\Debug"))return originalOpenKey(key,L"Software\\Microsoft",options,access,result);return status;}
inline LSTATUS WINAPI QueryValueHook(HKEY key,LPCWSTR name,LPDWORD reserved,LPDWORD type,LPBYTE data,LPDWORD bytes){if(!compositionDisabled||!name||_wcsicmp(name,L"DisableCompositionDiag"))return originalQueryValue(key,name,reserved,type,data,bytes);if(type)*type=REG_DWORD;if(data&&(!bytes||*bytes<sizeof(DWORD))){if(bytes)*bytes=sizeof(DWORD);return ERROR_MORE_DATA;}if(data)*reinterpret_cast<DWORD*>(data)=1;if(bytes)*bytes=sizeof(DWORD);return ERROR_SUCCESS;}
inline void WindowCreated(HWND window,LPCWSTR name){if(window&&reinterpret_cast<ULONG_PTR>(name)>0xffff&&(!_wcsicmp(name,L"ControlCenterWindow")||!_wcsicmp(name,L"Windows.UI.Core.CoreWindow"))&&!runtime)StartBridge();}
using CreateBand=HWND(WINAPI*)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,void*,DWORD);
using CreateBandEx=HWND(WINAPI*)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,void*,DWORD,DWORD);
inline CreateBand originalBand=nullptr;inline CreateBandEx originalBandEx=nullptr;
inline HWND WINAPI Band(DWORD ex,LPCWSTR type,LPCWSTR title,DWORD style,int x,int y,int w,int h,HWND parent,HMENU menu,HINSTANCE instance,void* param,DWORD band){auto window=originalBand(ex,type,title,style,x,y,w,h,parent,menu,instance,param,band);WindowCreated(window,type);return window;}
inline HWND WINAPI BandEx(DWORD ex,LPCWSTR type,LPCWSTR title,DWORD style,int x,int y,int w,int h,HWND parent,HMENU menu,HINSTANCE instance,void* param,DWORD band,DWORD flags){auto window=originalBandEx(ex,type,title,style,x,y,w,h,parent,menu,instance,param,band,flags);WindowCreated(window,type);return window;}
inline bool InitializeHost(){stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stopEvent)return false;auto kernel=GetModuleHandleW(L"kernelbase.dll"),user=GetModuleHandleW(L"user32.dll");bool ok=true;
 ok&=WindhawkUtils::SetFunctionHook(reinterpret_cast<OpenKey>(GetProcAddress(kernel,"RegOpenKeyExW")),OpenKeyHook,&originalOpenKey);
 ok&=WindhawkUtils::SetFunctionHook(reinterpret_cast<QueryValue>(GetProcAddress(kernel,"RegQueryValueExW")),QueryValueHook,&originalQueryValue);
 if(auto p=GetProcAddress(user,"CreateWindowInBand"))ok&=WindhawkUtils::SetFunctionHook(reinterpret_cast<CreateBand>(p),Band,&originalBand);
 if(auto p=GetProcAddress(user,"CreateWindowInBandEx"))ok&=WindhawkUtils::SetFunctionHook(reinterpret_cast<CreateBandEx>(p),BandEx,&originalBandEx);
 return ok;
}
}

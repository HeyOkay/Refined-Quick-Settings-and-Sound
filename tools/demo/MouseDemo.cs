using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;

public sealed class DemoPoint { public int X; public int Y; }
public sealed class DemoEvent { public double Ms; public int X; public int Y; public uint Message; public int Wheel; }
public static class DemoMouse {
    [StructLayout(LayoutKind.Sequential)] struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] struct HookData { public POINT Point; public uint MouseData, Flags, Time; public UIntPtr Extra; }
    [StructLayout(LayoutKind.Sequential)] struct MSG { public IntPtr Window; public uint Message; public UIntPtr WParam; public IntPtr LParam; public uint Time; public POINT Point; public uint Private; }
    delegate IntPtr HookProc(int code, IntPtr message, IntPtr data);
    [DllImport("user32.dll")] static extern IntPtr SetWindowsHookEx(int type, HookProc callback, IntPtr module, uint thread);
    [DllImport("user32.dll")] static extern bool UnhookWindowsHookEx(IntPtr hook);
    [DllImport("user32.dll")] static extern IntPtr CallNextHookEx(IntPtr hook, int code, IntPtr message, IntPtr data);
    [DllImport("kernel32.dll")] static extern IntPtr GetModuleHandle(string name);
    [DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
    [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT point);
    [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] static extern void mouse_event(uint flags, uint x, uint y, uint data, UIntPtr extra);
    [DllImport("user32.dll")] static extern bool PeekMessage(out MSG message, IntPtr window, uint min, uint max, uint remove);
    [DllImport("user32.dll")] static extern bool TranslateMessage(ref MSG message);
    [DllImport("user32.dll")] static extern IntPtr DispatchMessage(ref MSG message);
    [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("winmm.dll")] static extern uint timeBeginPeriod(uint period);
    [DllImport("winmm.dll")] static extern uint timeEndPeriod(uint period);
    static readonly List<DemoEvent> events = new List<DemoEvent>();
    static readonly HookProc callback = OnMouse;
    static readonly Stopwatch clock = new Stopwatch();
    static IntPtr hook;
    static double lastMove;
    public static double DurationMs;
    public static void Init() { SetThreadDpiAwarenessContext(new IntPtr(-4)); }
    static bool Down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }
    static void Pump() { MSG m; while(PeekMessage(out m, IntPtr.Zero, 0, 0, 1)) { TranslateMessage(ref m); DispatchMessage(ref m); } }
    static void AbortCheck() { if(Down(0x78)) throw new OperationCanceledException("Stopped with F9"); }
    public static DemoPoint WaitF8() {
        while(Down(0x77)) { Pump(); Thread.Sleep(4); }
        while(!Down(0x77)) { AbortCheck(); Pump(); Thread.Sleep(4); }
        POINT p; if(!GetCursorPos(out p)) throw new InvalidOperationException("Cannot read cursor position");
        while(Down(0x77)) { Pump(); Thread.Sleep(4); }
        return new DemoPoint { X=p.X, Y=p.Y };
    }
    static IntPtr OnMouse(int code, IntPtr message, IntPtr data) {
        if(code>=0 && clock.IsRunning) {
            uint m=unchecked((uint)message.ToInt64());
            if(m==0x200 || m==0x201 || m==0x202 || m==0x204 || m==0x205 || m==0x20A) {
                HookData d=(HookData)Marshal.PtrToStructure(data,typeof(HookData));
                double at=clock.Elapsed.TotalMilliseconds;
                if(m!=0x200 || at-lastMove>=8) {
                    if(m==0x200) lastMove=at;
                    events.Add(new DemoEvent { Ms=at, X=d.Point.X, Y=d.Point.Y, Message=m, Wheel=m==0x20A?(short)(d.MouseData>>16):0 });
                }
            }
        }
        return CallNextHookEx(hook,code,message,data);
    }
    public static DemoEvent[] Record(int maxSeconds) {
        events.Clear(); lastMove=-100;
        POINT p; GetCursorPos(out p);
        events.Add(new DemoEvent { Ms=0, X=p.X, Y=p.Y, Message=0x200 });
        hook=SetWindowsHookEx(14,callback,GetModuleHandle(null),0);
        if(hook==IntPtr.Zero) throw new InvalidOperationException("Cannot install the mouse recorder hook");
        clock.Restart();
        try {
            while(!Down(0x77) && clock.Elapsed.TotalSeconds<maxSeconds) { AbortCheck(); Pump(); Thread.Sleep(1); }
            DurationMs=clock.Elapsed.TotalMilliseconds;
            return events.ToArray();
        } finally { clock.Stop(); UnhookWindowsHookEx(hook); hook=IntPtr.Zero; }
    }
    static double Curve(double a,double b,double c,double d,double t) {
        double t2=t*t,t3=t2*t;
        double v=.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t2+(-a+3*b-3*c+d)*t3);
        return Math.Max(Math.Min(b,c), Math.Min(Math.Max(b,c), v));
    }
    static void Place(DemoEvent[] list,int at,double now) {
        DemoEvent b=list[at], c=list[Math.Min(at+1,list.Length-1)];
        double t=c.Ms>b.Ms?Math.Max(0,Math.Min(1,(now-b.Ms)/(c.Ms-b.Ms))):1;
        DemoEvent a=list[Math.Max(0,at-1)], d=list[Math.Min(list.Length-1,at+2)];
        SetCursorPos((int)Math.Round(Curve(a.X,b.X,c.X,d.X,t)),(int)Math.Round(Curve(a.Y,b.Y,c.Y,d.Y,t)));
    }
    static void Act(DemoEvent e) {
        uint flag=e.Message==0x201?2u:e.Message==0x202?4u:e.Message==0x204?8u:e.Message==0x205?16u:e.Message==0x20A?0x800u:0u;
        if(flag!=0) { SetCursorPos(e.X,e.Y); mouse_event(flag,0,0,unchecked((uint)e.Wheel),UIntPtr.Zero); }
    }
    public static void Replay(DemoEvent[] list,double duration) {
        if(list==null || list.Length<2) throw new ArgumentException("The mouse script is empty");
        timeBeginPeriod(1); var timer=Stopwatch.StartNew(); int next=0, at=0; double frame=0;
        try {
            while(timer.Elapsed.TotalMilliseconds<=duration+200) {
                AbortCheck(); double now=timer.Elapsed.TotalMilliseconds;
                while(next<list.Length && list[next].Ms<=now) { Act(list[next]); at=next++; }
                if(now>=frame) { Place(list,at,now); frame=now+1000.0/120; }
                Pump(); Thread.Sleep(1);
            }
        } finally {
            mouse_event(4|16,0,0,0,UIntPtr.Zero); timeEndPeriod(1);
        }
    }
}

#include "d3dhook.h"
#include "hooks.h"
#include "console.h"
#include "filelog.h"
#include "../bot/bot.h"

#include <Windows.h>
#include <d3d9.h>

// d3d9.lib'e link gerekmesin diye Direct3DCreate9'u dinamik yukluyoruz.

namespace D3DHook {

typedef HRESULT(__stdcall* tEndScene)(IDirect3DDevice9*);
typedef HRESULT(__stdcall* tPresent)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef IDirect3D9*(WINAPI* tDirect3DCreate9)(UINT);

static void*     g_gateEnd     = nullptr;
static void*     g_gatePresent = nullptr;
static uintptr_t g_addrEnd     = 0;
static uintptr_t g_addrPresent = 0;
static bool      g_installed   = false;

// Her iki hook da OnFrame'i cagirir; OnFrame kendi icinde throttle +
// setup-guard yaptigi icin cift cagri zararsiz.
static void RunFrame() {
    __try {
        Bot::Instance().OnFrame();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // bot patlarsa oyunu oldurme
    }
}

static HRESULT __stdcall HookedEndScene(IDirect3DDevice9* device) {
    static bool first = true;
    if (first) { first = false; FileLog::Write("[D3D] HookedEndScene ILK CAGRI — EndScene calisiyor!"); }
    RunFrame();
    return reinterpret_cast<tEndScene>(g_gateEnd)(device);
}

static HRESULT __stdcall HookedPresent(IDirect3DDevice9* device,
        const RECT* a, const RECT* b, HWND c, const RGNDATA* d) {
    static bool first = true;
    if (first) { first = false; FileLog::Write("[D3D] HookedPresent ILK CAGRI — Present calisiyor!"); }
    RunFrame();
    return reinterpret_cast<tPresent>(g_gatePresent)(device, a, b, c, d);
}

// Gecici D3D9 cihazi olusturup vtable'dan Present(17) ve EndScene(42)
// adreslerini okur. Tum cihazlar ayni d3d9.dll fonksiyonlarini paylasir.
static bool ResolveAddrs() {
    HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
    if (!d3d9) { FileLog::Write("[D3D] d3d9.dll yuklu degil"); return false; }

    auto pCreate = reinterpret_cast<tDirect3DCreate9>(GetProcAddress(d3d9, "Direct3DCreate9"));
    if (!pCreate) { FileLog::Write("[D3D] Direct3DCreate9 yok"); return false; }

    IDirect3D9* d3d = pCreate(D3D_SDK_VERSION);
    if (!d3d) { FileLog::Write("[D3D] Direct3DCreate9 NULL"); return false; }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed         = TRUE;
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow    = GetDesktopWindow();

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
        GetDesktopWindow(), D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    if (FAILED(hr) || !dev) {
        FileLog::Write("[D3D] CreateDevice basarisiz hr=0x%08lX", (unsigned long)hr);
        d3d->Release();
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(dev);
    g_addrPresent = reinterpret_cast<uintptr_t>(vtable[17]); // Present
    g_addrEnd     = reinterpret_cast<uintptr_t>(vtable[42]); // EndScene

    dev->Release();
    d3d->Release();

    FileLog::Write("[D3D] Present @ 0x%p  EndScene @ 0x%p",
                   (void*)g_addrPresent, (void*)g_addrEnd);
    Console::Log("[D3D] Present @ 0x%p  EndScene @ 0x%p",
                 (void*)g_addrPresent, (void*)g_addrEnd);
    return true;
}

bool Install() {
    if (g_installed) return true;
    if (!ResolveAddrs()) return false;

    bool anyHook = false;
    if (g_addrEnd && Hooks::Install(g_addrEnd,
            reinterpret_cast<uintptr_t>(HookedEndScene), &g_gateEnd)) {
        FileLog::Write("[D3D] EndScene hooklandi");
        anyHook = true;
    }
    if (g_addrPresent && Hooks::Install(g_addrPresent,
            reinterpret_cast<uintptr_t>(HookedPresent), &g_gatePresent)) {
        FileLog::Write("[D3D] Present hooklandi");
        anyHook = true;
    }

    g_installed = anyHook;
    return anyHook;
}

void Remove() {
    if (g_gateEnd && g_addrEnd)         Hooks::Remove(g_addrEnd, g_gateEnd);
    if (g_gatePresent && g_addrPresent) Hooks::Remove(g_addrPresent, g_gatePresent);
    g_gateEnd = g_gatePresent = nullptr;
    g_installed = false;
}

bool IsInstalled() { return g_installed; }

} // namespace D3DHook

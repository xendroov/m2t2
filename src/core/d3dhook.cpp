#include "d3dhook.h"
#include "hooks.h"
#include "console.h"
#include "filelog.h"
#include "../bot/bot.h"

#include <Windows.h>
#include <d3d9.h>

// d3d9.lib'e link gerekmesin diye Direct3DCreate9'u dinamik yukluyoruz.
// (d3d9.dll zaten oyun tarafindan yuklenmis durumda.)

namespace D3DHook {

typedef HRESULT(__stdcall* tEndScene)(IDirect3DDevice9*);
typedef IDirect3D9*(WINAPI* tDirect3DCreate9)(UINT);

static void*     g_gate         = nullptr;   // orijinal ilk 5 byte trampolin
static uintptr_t g_endSceneAddr = 0;
static bool      g_installed    = false;

// EndScene hook'u — ANA THREAD'de, her karede calisir.
static HRESULT __stdcall HookedEndScene(IDirect3DDevice9* device) {
    __try {
        Bot::Instance().OnFrame();   // bot mantigi ana thread'de guvenle calisir
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Bot kodu patlarsa oyunu oldurme — sessizce gec
    }
    return reinterpret_cast<tEndScene>(g_gate)(device);
}

// Gecici bir D3D9 cihazi olusturup vtable'dan EndScene adresini okur.
// Tum cihazlar ayni vtable fonksiyon adreslerini paylasir (d3d9.dll icinde),
// bu yuzden gecici cihazdan okudugumuz adres oyunun cihazi icin de gecerli.
static void* ResolveEndScene() {
    HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
    if (!d3d9) {
        FileLog::Write("[D3D] d3d9.dll yuklu degil");
        return nullptr;
    }
    auto pCreate = reinterpret_cast<tDirect3DCreate9>(GetProcAddress(d3d9, "Direct3DCreate9"));
    if (!pCreate) {
        FileLog::Write("[D3D] Direct3DCreate9 bulunamadi");
        return nullptr;
    }

    IDirect3D9* d3d = pCreate(D3D_SDK_VERSION);
    if (!d3d) {
        FileLog::Write("[D3D] Direct3DCreate9 NULL dondu");
        return nullptr;
    }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed         = TRUE;
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow    = GetDesktopWindow();

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, GetDesktopWindow(),
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &pp, &dev);

    if (FAILED(hr) || !dev) {
        FileLog::Write("[D3D] CreateDevice basarisiz (hr=0x%08lX)", (unsigned long)hr);
        d3d->Release();
        return nullptr;
    }

    void** vtable = *reinterpret_cast<void***>(dev);
    void* endScene = vtable[42];   // IDirect3DDevice9::EndScene vtable index

    dev->Release();
    d3d->Release();
    return endScene;
}

bool Install() {
    if (g_installed) return true;

    void* addr = ResolveEndScene();
    if (!addr) {
        FileLog::Write("[D3D] EndScene adresi cozulemedi");
        return false;
    }
    g_endSceneAddr = reinterpret_cast<uintptr_t>(addr);
    FileLog::Write("[D3D] EndScene @ 0x%p", addr);
    Console::Log("[D3D] EndScene @ 0x%p", addr);

    if (!Hooks::Install(g_endSceneAddr, reinterpret_cast<uintptr_t>(HookedEndScene), &g_gate)) {
        FileLog::Write("[D3D] EndScene hook kurulamadi");
        return false;
    }

    g_installed = true;
    FileLog::Write("[D3D] EndScene hooklandi — bot ana thread'de calisacak");
    Console::Log("[D3D] EndScene hooklandi");
    return true;
}

void Remove() {
    if (g_gate && g_endSceneAddr) {
        Hooks::Remove(g_endSceneAddr, g_gate);
        g_gate = nullptr;
    }
    g_installed = false;
}

bool IsInstalled() { return g_installed; }

} // namespace D3DHook

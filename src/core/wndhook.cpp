#include "wndhook.h"
#include "console.h"
#include "filelog.h"
#include "../bot/bot.h"

#include <Windows.h>
#include <cstring>

namespace WndHook {

static HWND     g_wnd      = nullptr;
static WNDPROC  g_orig     = nullptr;
static bool     g_unicode  = false;
static bool     g_installed = false;
static const UINT_PTR TIMER_ID = 0xB0B0;

static void RunFrame() {
    __try {
        Bot::Instance().OnFrame();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // bot patlarsa oyunu oldurme
    }
}

static LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static bool first = true;
    if (first) {
        first = false;
        FileLog::Write("[Wnd] HookedWndProc ILK CAGRI — ANA THREAD'deyiz");
        SetTimer(hwnd, TIMER_ID, 100, nullptr); // idle'da da tetiklensin
    }

    if (msg == WM_TIMER && wp == TIMER_ID) {
        RunFrame();
        return 0;
    }

    // Aktif oyunda bol mesaj gelir — her mesajda (throttle'li) calistir
    RunFrame();

    return CallWindowProc(g_orig, hwnd, msg, wp, lp);
}

// Process'in TUM ust-duzey pencerelerini tarar, konsolu atlar,
// EN BUYUK pencereyi (oyun render penceresi) secer.
struct Candidate { HWND best; long bestArea; };

static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam) {
    auto* c = reinterpret_cast<Candidate*>(lParam);

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    if (!IsWindowVisible(hwnd))       return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE; // sadece ust-duzey
    if (hwnd == GetConsoleWindow())   return TRUE;         // kendi konsolumuz degil

    char cls[64] = {0};
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strcmp(cls, "ConsoleWindowClass") == 0) return TRUE; // konsol sinifi degil

    RECT r = {0};
    GetWindowRect(hwnd, &r);
    long w = r.right - r.left, h = r.bottom - r.top;
    long area = w * h;

    char title[256] = {0};
    GetWindowTextA(hwnd, title, sizeof(title));
    FileLog::Write("[Wnd] aday: '%s' cls=%s %ldx%ld", title, cls, w, h);

    if (area > c->bestArea) { c->bestArea = area; c->best = hwnd; }
    return TRUE; // devam et — en buyugu sec
}

bool Install() {
    if (g_installed) return true;

    Candidate cand = { nullptr, 0 };
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&cand));
    HWND hwnd = cand.best;
    if (!hwnd) {
        FileLog::Write("[Wnd] oyun penceresi bulunamadi");
        Console::Log("[Wnd] HATA: pencere bulunamadi");
        return false;
    }
    g_wnd = hwnd;

    char title[256] = {0};
    GetWindowTextA(hwnd, title, sizeof(title));
    FileLog::Write("[Wnd] SECILEN pencere: '%s' hwnd=0x%p", title, (void*)hwnd);
    Console::Log("[Wnd] pencere: '%s'", title);

    g_unicode = IsWindowUnicode(hwnd) != FALSE;
    if (g_unicode)
        g_orig = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                 reinterpret_cast<LONG_PTR>(HookedWndProc)));
    else
        g_orig = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(hwnd, GWLP_WNDPROC,
                 reinterpret_cast<LONG_PTR>(HookedWndProc)));

    if (!g_orig) {
        FileLog::Write("[Wnd] SetWindowLongPtr basarisiz (err=%lu)", GetLastError());
        Console::Log("[Wnd] HATA: subclass basarisiz");
        return false;
    }

    g_installed = true;
    FileLog::Write("[Wnd] WndProc hooklandi (unicode=%d)", g_unicode ? 1 : 0);
    Console::Log("[Wnd] WndProc hooklandi — bot ana thread'de calisacak");
    return true;
}

void Remove() {
    if (g_wnd && g_orig) {
        KillTimer(g_wnd, TIMER_ID);
        if (g_unicode)
            SetWindowLongPtrW(g_wnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_orig));
        else
            SetWindowLongPtrA(g_wnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_orig));
        g_orig = nullptr;
    }
    g_installed = false;
}

bool IsInstalled() { return g_installed; }

void SendKey(int vk) {
    if (!g_wnd) return;
    UINT sc = MapVirtualKeyA(vk, 0 /*MAPVK_VK_TO_VSC*/);
    LPARAM down = 1 | (static_cast<LPARAM>(sc) << 16);
    LPARAM up   = 1 | (static_cast<LPARAM>(sc) << 16) | (1L << 30) | (1L << 31);
    PostMessageA(g_wnd, WM_KEYDOWN, vk, down);
    PostMessageA(g_wnd, WM_KEYUP,   vk, up);
}

} // namespace WndHook

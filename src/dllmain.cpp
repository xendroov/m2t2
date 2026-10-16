#include <Windows.h>
#include "core/filelog.h"
#include "core/console.h"
#include "core/wndhook.h"
#include "game/pyapi.h"
#include "network/packet_logger.h"
#include "bot/bot.h"

static HMODULE g_hModule = nullptr;

static DWORD WINAPI MainThread(LPVOID) {
    FileLog::Write("=== DLL yuklendi. PID=%u ===", GetCurrentProcessId());

    // Oyunun Python ortamini tam baslat — erken dokunma crash yapar
    Sleep(3000);

    Console::Open();
    Console::Log("M2T2 Bot  |  INSERT=ac/kapat  DELETE=cikis");
    FileLog::Write("Console acildi");

    // python27.dll fonksiyon pointer'larini al
    if (!PyAPI::Init()) {
        FileLog::Write("PyAPI::Init BASARISIZ — oyun kapatiliyor mu?");
        Console::Log("HATA: python27.dll bulunamadi");
        // Yine de klavye dongusune gir ki DELETE ile cikabilelim
        goto keyboard_loop;
    }
    FileLog::Write("PyAPI hazir");

    // WndProc hook'u kur — bot ANA THREAD'de (mesaj dongusu) calisacak.
    // D3D hook bu client'ta tetiklenmedi; WndProc her Win32 penceresinde calisir.
    FileLog::Write("WndHook::Install...");
    if (WndHook::Install()) {
        Console::Log("[Wnd] Hook aktif — bot ana thread'de guvenli calisir");
        FileLog::Write("WndHook kuruldu");
    } else {
        Console::Log("[Wnd] HATA: pencere hooklanamadi");
        FileLog::Write("WndHook BASARISIZ — bot calismayabilir");
    }

    // PacketLogger: SEND/RECV adresleri 0 oldugu icin sessizce atlaniyor
    PacketLogger::InstallHooks();

keyboard_loop:
    FileLog::Write("Klavye dongusu basliyor");

    while (true) {
        // INSERT — bot ac/kapat. Python'a BU thread'den dokunmuyoruz;
        // sadece bayragi ceviriyoruz. Gercek is EndScene hook'unda (ana thread).
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            if (Bot::Instance().IsRunning()) {
                Bot::Instance().Stop();
                FileLog::Write("Bot durduruldu");
            } else {
                Bot::Instance().Start();
                FileLog::Write("Bot baslatildi");
            }
        }

        // END — bellek dump'i al (analiz icin)
        if (GetAsyncKeyState(VK_END) & 1) {
            Console::Log("[Dump] istendi — bir sonraki karede yazilacak");
            FileLog::Write("END — dump istendi");
            Bot::Instance().RequestDump();
        }

        // DELETE — DLL'i kaldir
        if (GetAsyncKeyState(VK_DELETE) & 1) {
            FileLog::Write("DELETE — cikiliyor");
            break;
        }

        Sleep(50);
    }

    Bot::Instance().Stop();
    WndHook::Remove();
    // WndProc'u geri yukledikten sonra, ucustaki cagrilar HookedWndProc'tan
    // cikana kadar bekle — yoksa DLL bosaltilirken crash olur.
    Sleep(300);
    Bot::Instance().ReleaseShared();  // artik OnFrame cagrilmiyor — guvenli
    PacketLogger::RemoveHooks();
    Console::Log("DLL kaldirildi.");
    FileLog::Write("DLL kaldirildi");
    Console::Close();

    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_hModule = hModule;
        FileLog::Write("DllMain DLL_PROCESS_ATTACH");
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}

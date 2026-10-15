#include <Windows.h>
#include "core/filelog.h"
#include "core/console.h"
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

    // PacketLogger: SEND/RECV adresleri 0 oldugu icin sessizce atlaniyor
    PacketLogger::InstallHooks();

keyboard_loop:
    FileLog::Write("Klavye dongusu basliyor");

    {
        DWORD lastTick = 0;
        while (true) {
            // INSERT — bot ac/kapat
            if (GetAsyncKeyState(VK_INSERT) & 1) {
                if (Bot::Instance().IsRunning()) {
                    Bot::Instance().Stop();
                    FileLog::Write("Bot durduruldu");
                } else {
                    Bot::Instance().Start();
                    FileLog::Write("Bot baslatildi");
                }
            }

            // Bot aktifse her 150ms'de bir saldir
            DWORD now = GetTickCount();
            if (Bot::Instance().IsRunning() && (now - lastTick) >= 150) {
                Bot::Instance().Tick();
                lastTick = now;
            }

            // DELETE — DLL'i kaldir
            if (GetAsyncKeyState(VK_DELETE) & 1) {
                FileLog::Write("DELETE — cikiliyor");
                break;
            }

            Sleep(50);
        }
    }

    Bot::Instance().Stop();
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

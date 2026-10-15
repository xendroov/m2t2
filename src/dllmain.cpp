#include <Windows.h>
#include "core/filelog.h"
#include "core/console.h"
#include "game/pyapi.h"
#include "network/packet_logger.h"
#include "bot/bot.h"

// hModule DllMain'den saklaniyor — FreeLibraryAndExitThread icin
static HMODULE g_hModule = nullptr;

static DWORD WINAPI MainThread(LPVOID) {
    // En önce dosyaya yaz — konsol bile açılmadan önce
    FileLog::Write("=== DLL yuklendi. PID=%u ===", GetCurrentProcessId());

    // Oyunun Python ortamini tam olarak baslatmasini bekle.
    // Erken Python teması → import lock deadlock → oyun kapanir.
    FileLog::Write("Oyun baslangic bekleniyor (3s)...");
    Sleep(3000);
    FileLog::Write("Bekleme tamam");

    FileLog::Write("AllocConsole...");
    Console::Open();
    FileLog::Write("Console::Open tamam");

    Console::Log("M2T2 Bot yuklendi. PID=%u", GetCurrentProcessId());

    FileLog::Write("PyAPI::Init cagiriliyor...");
    if (!PyAPI::Init()) {
        FileLog::Write("PyAPI::Init BASARISIZ");
        Console::Log("PyAPI baslatma basarisiz!");
        Console::Log("Yuklü Python DLL'leri:");

        const char* names[] = {
            "python27.dll","python26.dll","python24.dll",
            "python.dll","python3.dll", nullptr
        };
        for (int i = 0; names[i]; ++i) {
            HMODULE h = GetModuleHandleA(names[i]);
            if (h) {
                Console::Log("  BULUNDU: %s @ 0x%p", names[i], (void*)h);
                FileLog::Write("  BULUNDU: %s @ 0x%p", names[i], (void*)h);
            } else {
                FileLog::Write("  yok: %s", names[i]);
            }
        }

        Console::Log("Devam ediliyor (PyAPI olmadan)...");
        FileLog::Write("PyAPI olmadan devam ediliyor");
        goto keyboard_loop;
    }

    FileLog::Write("PyAPI::Init basarili");
    Console::Log("[PyAPI] Hazir");

    // ImportModules BURADAN KALDIRILDI.
    // player/net/chr modülleri oyun baslatirken zaten __main__'e import edilmis olur.
    // Bot basladiginda (INSERT) bot.cpp kendi Python kodunda import ediyor.
    // Startup'ta import yapmak Python 2.7 import lock'u ile deadlock yapabilir.

    FileLog::Write("PacketLogger::InstallHooks...");
    PacketLogger::InstallHooks();
    FileLog::Write("PacketLogger tamam (SEND/RECV adresleri 0 ise atlanir)");

    FileLog::Write("Bot hazir (INSERT=basla DELETE=cikis)");

keyboard_loop:
    Console::Log("INSERT=bot ac/kapat  DELETE=cikis");
    FileLog::Write("Klavye dongusu basliyor");

    DWORD lastBotTick = 0;
    while (true) {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            if (Bot::Instance().IsRunning()) {
                Bot::Instance().Stop();
                Console::Log("Bot DURDURULDU");
                FileLog::Write("Bot durduruldu");
            } else {
                Bot::Instance().Start();
                Console::Log("Bot BASLATILDI");
                FileLog::Write("Bot baslatildi");
            }
        }

        // Bot aktifse her ~150ms'de bir tick cal
        DWORD now = GetTickCount();
        if (Bot::Instance().IsRunning() && (now - lastBotTick) >= 150) {
            Bot::Instance().Tick();
            lastBotTick = now;
        }

        if (GetAsyncKeyState(VK_DELETE) & 1) {
            FileLog::Write("DELETE - cikiliyor");
            break;
        }
        Sleep(50);
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

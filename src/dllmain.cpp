#include <Windows.h>
#include "core/filelog.h"
#include "core/console.h"
#include "game/pyapi.h"
#include "network/packet_logger.h"
#include "bot/bot.h"

static DWORD WINAPI MainThread(LPVOID) {
    // En önce dosyaya yaz — konsol bile açılmadan önce
    FileLog::Write("=== DLL yuklendi. PID=%u ===", GetCurrentProcessId());

    FileLog::Write("AllocConsole...");
    Console::Open();
    FileLog::Write("Console::Open tamam");

    Console::Log("DLL yuklendi. PID=%u", GetCurrentProcessId());

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
        // Crash yerine devam et — klavye döngüsüne gir
        goto keyboard_loop;
    }

    FileLog::Write("PyAPI::Init basarili");
    Console::Log("[PyAPI] Hazir");

    FileLog::Write("PacketLogger::InstallHooks...");
    PacketLogger::InstallHooks();
    FileLog::Write("PacketLogger tamam");

    FileLog::Write("Bot::Start...");
    Bot::Instance().Start();
    FileLog::Write("Bot baslatildi");

keyboard_loop:
    Console::Log("INSERT=bot ac/kapat  DELETE=cikis");
    FileLog::Write("Klavye dongusu basliyor");

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

    FreeLibraryAndExitThread(GetModuleHandleA("m2t2bot.dll"), 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // DllMain'de minimal iş yap — thread aç
        DisableThreadLibraryCalls(hModule);
        FileLog::Write("DllMain DLL_PROCESS_ATTACH");
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}

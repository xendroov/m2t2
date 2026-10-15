#include <Windows.h>
#include "core/console.h"
#include "game/pyapi.h"
#include "network/packet_logger.h"
#include "bot/bot.h"

// -----------------------------------------------------------------------
// DLL inject olduktan sonra burası çağrılır.
// Inject araçları: RemoteDLL, Process Hacker, manuel CreateRemoteThread.
// -----------------------------------------------------------------------

static DWORD WINAPI MainThread(LPVOID) {
    Console::Open();
    Console::Log("DLL yuklendi. PID=%u", GetCurrentProcessId());

    // 1) Python API'yi başlat (python27.dll bağlantısı)
    if (!PyAPI::Init()) {
        Console::Log("PyAPI baslatma basarisiz!");
        Console::Log("Yuklü Python DLL'leri kontrol ediliyor...");

        // Hangi Python DLL'leri process'te var, listele
        const char* names[] = {
            "python27.dll","python26.dll","python24.dll",
            "python.dll","python3.dll",nullptr
        };
        bool found = false;
        for (int i = 0; names[i]; ++i) {
            HMODULE h = GetModuleHandleA(names[i]);
            if (h) { Console::Log("  BULUNDU: %s @ 0x%p", names[i], (void*)h); found = true; }
            else    Console::Log("  yok: %s", names[i]);
        }
        if (!found)
            Console::Log("Hic Python DLL bulunamadi — client Python kullanmiyor olabilir.");

        Console::Log("Cikis icin bir tusa basin...");
        system("pause");
        Console::Close();
        FreeLibraryAndExitThread(GetModuleHandleA("m2t2bot.dll"), 0);
        return 0;
    }

    // 2) Packet hook'larını kur (önce log, sonra bot)
    PacketLogger::InstallHooks();

    // 3) Botu başlat
    Bot::Instance().Start();

    // 3) Klavye kısayolları (Insert = durdur/başlat, Delete = eject)
    while (true) {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            if (Bot::Instance().IsRunning()) {
                Bot::Instance().Stop();
                Console::Log("Bot DURDURULDU (INSERT)");
            } else {
                Bot::Instance().Start();
                Console::Log("Bot BASLATILDI (INSERT)");
            }
        }
        if (GetAsyncKeyState(VK_DELETE) & 1) {
            Console::Log("Eject ediliyor...");
            break;
        }
        Sleep(50);
    }

    // Temizlik
    Bot::Instance().Stop();
    PacketLogger::RemoveHooks();

    Console::Log("DLL kaldirildi. Paket istatistigi: send=%llu recv=%llu",
        PacketLogger::SentCount(), PacketLogger::RecvCount());
    Console::Close();

    FreeLibraryAndExitThread(
        GetModuleHandleA("m2t2bot.dll"), 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}

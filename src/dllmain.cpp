#include <Windows.h>
#include "core/console.h"
#include "network/packet_logger.h"
#include "bot/bot.h"

// -----------------------------------------------------------------------
// DLL inject olduktan sonra burası çağrılır.
// Inject araçları: RemoteDLL, Process Hacker, manuel CreateRemoteThread.
// -----------------------------------------------------------------------

static DWORD WINAPI MainThread(LPVOID) {
    Console::Open();
    Console::Log("DLL yuklendi. PID=%u", GetCurrentProcessId());

    // 1) Packet hook'larını kur (önce log, sonra bot)
    PacketLogger::InstallHooks();

    // 2) Botu başlat
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

#include "bot.h"
#include "states/farm_state.h"
#include "states/loot_state.h"
#include "../game/player.h"
#include "../core/console.h"
#include "../core/filelog.h"
#include <Windows.h>

Bot& Bot::Instance() {
    static Bot inst;
    return inst;
}

static DWORD WINAPI BotThread(LPVOID param) {
    reinterpret_cast<Bot*>(param)->Loop(); // çağrısını dışarı açmak için friend veya proxy
    return 0;
}

// Loop'u public yapıyoruz (dllmain içinden çağrılacak) — alternatif: friend
void Bot::Loop() {
    Console::Log("[Bot] Thread baslatildi");
    FileLog::Write("[Bot] Thread baslatildi");

    while (m_running.load()) {
        __try {
            if (m_state) {
                float hp = Player::GetHPPercent();
                if (hp > 0.f && hp < 10.f) {
                    Console::Log("[Bot] HP dusuk (%.0f%%) — bekleniyor...", hp);
                    Sleep(2000);
                    continue;
                }
                m_state->Update();
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            DWORD code = GetExceptionCode();
            Console::Log("[Bot] EXCEPTION 0x%08X — state durduruldu", code);
            FileLog::Write("[Bot] EXCEPTION 0x%08X icinde %s",
                code, m_state ? m_state->Name() : "?");
            m_state.reset();   // crash olan state'i sıfırla
            Sleep(1000);
        }
        Sleep(TICK_MS);
    }

    Console::Log("[Bot] Thread durdu");
    FileLog::Write("[Bot] Thread durdu");
}

void Bot::Start() {
    if (m_running.exchange(true)) return; // Zaten çalışıyor

    // Varsayılan state: farm + loot
    auto farm = std::make_unique<FarmState>(1500.f, /*skillVnum=*/0);
    SetState(std::move(farm));

    // PacketLogger'a loot state callback'ini kaydet (iç bağlantı için gerek duyulursa)
    // LootState bağımsız dinleyici olarak PacketLogger::OnRecv'e kendi OnEnter'ında kayıt olur.

    m_thread = CreateThread(nullptr, 0, BotThread, this, 0, nullptr);
    Console::Log("[Bot] Baslatildi");
}

void Bot::Stop() {
    m_running.store(false);
    if (m_thread) {
        WaitForSingleObject(m_thread, 3000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    if (m_state) { m_state->OnExit(); m_state.reset(); }
    Console::Log("[Bot] Durduruldu");
}

void Bot::SetState(std::unique_ptr<IState> state) {
    if (m_state) m_state->OnExit();
    m_state = std::move(state);
    if (m_state) m_state->OnEnter();
}

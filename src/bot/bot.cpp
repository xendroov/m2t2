#include "bot.h"
#include "../game/pyapi.h"
#include "../core/console.h"
#include "../core/filelog.h"
#include <cstdio>

Bot& Bot::Instance() {
    static Bot inst;
    return inst;
}

void Bot::Start() {
    if (m_running.exchange(true)) return;
    // Python'a BURADA dokunmuyoruz (bu bizim thread'imiz).
    // Gercek is OnFrame()'de, EndScene hook'u uzerinden ANA THREAD'de yapilir.
    Console::Log("[Bot] Baslatildi — ana thread'de calisacak (INSERT=dur)");
    FileLog::Write("[Bot] Start() — m_running=true");
}

void Bot::Stop() {
    if (!m_running.exchange(false)) return;
    Console::Log("[Bot] Durduruldu");
    FileLog::Write("[Bot] Stop()");
}

// Ilk karede bir kez: oyun modullerini __main__'e enjekte et + tani yap.
// ANA THREAD'de calisir (EndScene), bu yuzden guvenli.
void Bot::DoSetup() {
    FileLog::Write("[Bot] === OnFrame ilk calisma (ANA THREAD) ===");

    struct Pair { const char* sysName; const char* as; };
    Pair tryList[] = {
        {"playerm2g2", "player"}, {"m2netm2g", "net"},
        {"chrmgrm2g", "chrmgr"},  {"chr", "chr"},
        {"nonplayer", "nonplayer"},
    };
    for (auto& p : tryList) {
        if (PyAPI::InjectModule(p.sysName, p.as))
            FileLog::Write("[Bot] ENJEKTE: %s -> %s", p.sysName, p.as);
    }

    FileLog::Write("[Bot] player = %s", PyAPI::EvalStr("str(player)").c_str());
    FileLog::Write("[Bot] net    = %s", PyAPI::EvalStr("str(net)").c_str());
    FileLog::Write("[Bot] chrmgr = %s", PyAPI::EvalStr("str(chrmgr)").c_str());

    // GetStatus sabitlerini bul: 0..20 arasi hangisi sifir-disi deger donuyor?
    for (int i = 0; i <= 20; ++i) {
        char buf[64];
        snprintf(buf, sizeof(buf), "player.GetStatus(%d)", i);
        int v = PyAPI::EvalInt(buf);
        if (v != 0) FileLog::Write("[Bot] GetStatus(%d) = %d", i, v);
    }
    FileLog::Write("[Bot] GetTargetVID() = %d", PyAPI::EvalInt("player.GetTargetVID()"));
    FileLog::Write("[Bot] === SETUP BITTI — farm basliyor ===");

    Console::Log("[Bot] Setup tamam — farm aktif");
}

// D3D EndScene hook'tan her karede cagrilir — ANA THREAD.
void Bot::OnFrame() {
    if (!m_running.load()) return;

    if (!m_setup_done) {
        DoSetup();
        m_setup_done = true;
    }

    // ~150ms'de bir saldir (her kare degil)
    DWORD now = GetTickCount();
    if (now - m_lastTick < 150) return;
    m_lastTick = now;

    // Secili hedefe saldir
    int vid = PyAPI::EvalInt("player.GetTargetVID()");
    if (vid <= 0) return;

    char buf[96];
    snprintf(buf, sizeof(buf), "chrmgr.IsDead(%d)", vid);
    if (PyAPI::EvalBool(buf)) return;   // olmusse birak

    snprintf(buf, sizeof(buf), "net.SendOnClickPacket(%d)", vid);
    PyAPI::Exec(buf);                   // saldir
}

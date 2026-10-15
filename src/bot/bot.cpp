#include "bot.h"
#include "../game/pyapi.h"
#include "../core/console.h"
#include "../core/filelog.h"
#include <cstdio>

Bot& Bot::Instance() {
    static Bot inst;
    return inst;
}

// Metin2 POINT sabitleri — GetStatus taramasiyla DOGRULANDI (23:35 log):
//   1=LEVEL  3=EXP  4=NEXT_EXP  5=HP  6=MAX_HP  7=SP  8=MAX_SP

void Bot::Start() {
    if (m_running.exchange(true)) return;
    Console::Log("[Bot] Baslatildi — bir moba tikla, bot saldiracak (INSERT=dur)");
    FileLog::Write("[Bot] Start()");
}

void Bot::Stop() {
    if (!m_running.exchange(false)) return;
    Console::Log("[Bot] Durduruldu");
    FileLog::Write("[Bot] Stop()");
}

// Ilk karede bir kez (ANA THREAD): oyun modullerini __main__'e enjekte et.
void Bot::DoSetup() {
    FileLog::Write("[Bot] Setup (ANA THREAD) — moduller enjekte ediliyor");
    PyAPI::InjectModule("playerm2g2", "player");
    PyAPI::InjectModule("m2netm2g",   "net");
    PyAPI::InjectModule("chrmgrm2g",  "chrmgr");
    PyAPI::InjectModule("chr",        "chr");
    PyAPI::InjectModule("nonplayer",  "nonplayer");
    Console::Log("[Bot] Moduller hazir. Bir moba tikla -> otomatik vurur.");
    FileLog::Write("[Bot] Setup tamam");
}

// D3D degil — WndProc hook'tan her mesajda ANA THREAD'de cagrilir.
void Bot::OnFrame() {
    if (!m_running.load()) return;

    if (!m_setup_done) {
        DoSetup();
        m_setup_done = true;
    }

    DWORD now = GetTickCount();
    if (now - m_lastTick < 150) return;
    m_lastTick = now;

    int hp    = PyAPI::EvalInt("player.GetStatus(5)");   // HP
    int maxhp = PyAPI::EvalInt("player.GetStatus(6)");   // MAX_HP

    // 2 saniyede bir canli durum yaz (botun calistigini gorursun)
    if (now - m_lastStatus >= 2000) {
        m_lastStatus = now;
        int sp  = PyAPI::EvalInt("player.GetStatus(7)");
        int lvl = PyAPI::EvalInt("player.GetStatus(1)");
        int tvid = PyAPI::EvalInt("player.GetTargetVID()");
        Console::Log("[Bot] Lv%d  HP %d/%d  SP %d  hedef=%d", lvl, hp, maxhp, sp, tvid);
    }

    // HP kritikse saldirma (olmemek icin)
    if (maxhp > 0 && (hp * 100 / maxhp) < 10) return;

    // Secili hedefe saldir
    int vid = PyAPI::EvalInt("player.GetTargetVID()");
    if (vid <= 0) return;

    char buf[96];
    snprintf(buf, sizeof(buf), "chrmgr.IsDead(%d)", vid);
    if (PyAPI::EvalBool(buf)) return;   // olmusse birak

    snprintf(buf, sizeof(buf), "net.SendOnClickPacket(%d)", vid);
    PyAPI::Exec(buf);                   // saldir
}

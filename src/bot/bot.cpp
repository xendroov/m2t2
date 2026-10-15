#include "bot.h"
#include "../game/pyapi.h"
#include "../game/offsets.h"
#include "../core/console.h"
#include "../core/filelog.h"

Bot& Bot::Instance() {
    static Bot inst;
    return inst;
}

void Bot::Start() {
    if (m_running.exchange(true)) return;
    Console::Log("[Bot] Baslatildi — INSERT=dur DELETE=cikis");
    FileLog::Write("[Bot] Baslatildi");
}

void Bot::Stop() {
    if (!m_running.exchange(false)) return;
    Console::Log("[Bot] Durduruldu");
    FileLog::Write("[Bot] Durduruldu");
}

// PyRun_String / import / threading YOK.
// Python C extension fonksiyonlari dogrudan C duzeyde cagriliyor.
// Referans bot yaklasimi: direkt adres + GIL al + cagir + GIL birak.
void Bot::Tick() {
    // HP kontrol — POINT_HP=0, POINT_MAX_HP=1 (standart Metin2 sabitleri)
    int hp    = PyAPI::CallInt_Int(Offsets::Player::GetStatus, 0);
    int maxhp = PyAPI::CallInt_Int(Offsets::Player::GetStatus, 1);
    if (maxhp > 0 && (hp * 100 / maxhp) < 10) {
        // HP kritik — saldirisiz bekle
        return;
    }

    // chr.Pick() — en yakin hedefin VID'ini al
    int vid = PyAPI::CallInt_Void(Offsets::Chr::Pick);
    if (vid <= 0) return;

    // chrmgr.IsDead(vid) — olmusse atla
    if (PyAPI::CallBool_Int(Offsets::ChrMgr::IsDead, vid)) return;

    // chrmgr.IsPC(vid) — oyuncuysa dokunma
    if (PyAPI::CallBool_Int(Offsets::ChrMgr::IsPC, vid)) return;

    // net.SendOnClickPacket(vid) — saldir
    PyAPI::CallVoid_Int(Offsets::Net::SendOnClickPacket, vid);
}

#include "bot.h"
#include "../game/pyapi.h"
#include "../core/console.h"
#include "../core/filelog.h"
#include <cstdio>

Bot& Bot::Instance() {
    static Bot inst;
    return inst;
}

// -----------------------------------------------------------------------
// Sabit adres YOK. Fonksiyonlar isimle cagriliyor (player.GetStatus gibi).
// Yorumlayici dogru adresi kendi bulur — yanlis adrese atlayip crash OLMAZ.
// import YOK, threading YOK — sadece zaten yuklu modullere eriyoruz.
// -----------------------------------------------------------------------

void Bot::Start() {
    if (m_running.exchange(true)) return;

    // --- TANI: hangi moduller/fonksiyonlar erisilebilir? ---
    // Her eval crash-guvenli: modul yoksa "" veya 0 doner, cokmez.
    FileLog::Write("[Bot] === TANI BASLADI ===");

    std::string sPlayer = PyAPI::EvalStr("str(player)");
    FileLog::Write("[Bot] player     = %s", sPlayer.empty() ? "(YOK)" : sPlayer.c_str());

    std::string sChr = PyAPI::EvalStr("str(chr)");
    FileLog::Write("[Bot] chr        = %s", sChr.empty() ? "(YOK)" : sChr.c_str());

    std::string sChrmgr = PyAPI::EvalStr("str(chrmgr)");
    FileLog::Write("[Bot] chrmgr     = %s", sChrmgr.empty() ? "(YOK)" : sChrmgr.c_str());

    std::string sNet = PyAPI::EvalStr("str(net)");
    FileLog::Write("[Bot] net        = %s", sNet.empty() ? "(YOK)" : sNet.c_str());

    // HP okumayi dene — hem player.HP sabiti hem ham 0 ile
    std::string hpConst = PyAPI::EvalStr("str(player.GetStatus(player.HP))");
    FileLog::Write("[Bot] GetStatus(player.HP) = %s", hpConst.empty() ? "(HATA)" : hpConst.c_str());

    std::string hp0 = PyAPI::EvalStr("str(player.GetStatus(0))");
    FileLog::Write("[Bot] GetStatus(0)         = %s", hp0.empty() ? "(HATA)" : hp0.c_str());

    std::string target = PyAPI::EvalStr("str(player.GetTargetVID())");
    FileLog::Write("[Bot] GetTargetVID()       = %s", target.empty() ? "(HATA)" : target.c_str());

    FileLog::Write("[Bot] === TANI BITTI ===");

    Console::Log("[Bot] Baslatildi — tani C:\\m2t2bot_log.txt dosyasinda");
    FileLog::Write("[Bot] Baslatildi");
}

void Bot::Stop() {
    if (!m_running.exchange(false)) return;
    Console::Log("[Bot] Durduruldu");
    FileLog::Write("[Bot] Durduruldu");
}

void Bot::Tick() {
    // Hepsi isim-tabanli eval — crash-guvenli.
    // Hedef: oyuncunun secili hedefine saldir (varsa).
    int vid = PyAPI::EvalInt("player.GetTargetVID()");
    if (vid <= 0) return;

    char buf[96];
    snprintf(buf, sizeof(buf), "chrmgr.IsDead(%d)", vid);
    if (PyAPI::EvalBool(buf)) return;          // olmusse atla

    snprintf(buf, sizeof(buf), "net.SendOnClickPacket(%d)", vid);
    PyAPI::Exec(buf);                          // saldir
}

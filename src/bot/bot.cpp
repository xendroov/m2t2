#include "bot.h"
#include "../game/pyapi.h"
#include "../core/console.h"
#include "../core/filelog.h"

Bot& Bot::Instance() {
    static Bot inst;
    return inst;
}

// Bir kez cagrilir: sys.modules'tan modul referanslarini __main__'e yazar.
// 'import player' DEMİYORUZ — import lock kullanir, crash yapabilir.
// sys.modules.get() ile zaten yuklu modulleri aliyoruz.
static const char* BOT_SETUP_CODE = R"(
import sys as _sys
_m2bot_player  = _sys.modules.get('player')
_m2bot_net     = _sys.modules.get('net')
_m2bot_chr     = _sys.modules.get('chr')
_m2bot_chrmgr  = _sys.modules.get('chrmgr')
_m2bot_ok      = bool(_m2bot_player and _m2bot_net and _m2bot_chr and _m2bot_chrmgr)
)";

// Her tick'te cagrilir (~150ms). Threading yok — C++ thread'inden direkt calisir.
static const char* BOT_TICK_CODE = R"(
if _m2bot_ok:
    try:
        _hp    = _m2bot_player.GetStatus(_m2bot_player.HP)
        _maxhp = _m2bot_player.GetStatus(_m2bot_player.MAX_HP)
        if _maxhp > 0 and (_hp * 10) > _maxhp:
            _vid = _m2bot_chr.Pick()
            if _vid and _vid > 0:
                _dead = _m2bot_chrmgr.IsDead(_vid)
                _ispc = _m2bot_chrmgr.IsPC(_vid)
                if not _dead and not _ispc:
                    _m2bot_net.SendOnClickPacket(_vid)
    except Exception:
        pass
)";

void Bot::Start() {
    if (m_running.exchange(true)) return;

    if (!m_setup_done) {
        FileLog::Write("[Bot] Setup: moduller sys.modules'tan aliniyor...");
        PyAPI::Exec(BOT_SETUP_CODE);
        m_setup_done = true;

        bool ok = PyAPI::EvalBool("_m2bot_ok");
        if (ok) {
            Console::Log("[Bot] Moduller hazir: player/net/chr/chrmgr");
            FileLog::Write("[Bot] Moduller hazir");
        } else {
            Console::Log("[Bot] UYARI: Bazi moduller bulunamadi (_m2bot_ok=False)");
            FileLog::Write("[Bot] UYARI: moduller yok");
        }
    }

    Console::Log("[Bot] Baslatildi (INSERT=dur)");
    FileLog::Write("[Bot] Baslatildi");
}

void Bot::Stop() {
    if (!m_running.exchange(false)) return;
    Console::Log("[Bot] Durduruldu");
    FileLog::Write("[Bot] Durduruldu");
}

void Bot::Tick() {
    // C++ keyboard loop'undan cagrilir — threading yok, crash riski minimal
    PyAPI::Exec(BOT_TICK_CODE);
}

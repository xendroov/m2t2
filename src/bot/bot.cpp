#include "bot.h"
#include "../game/pyapi.h"
#include "../core/console.h"
#include "../core/filelog.h"

Bot& Bot::Instance() {
    static Bot inst;
    return inst;
}

// Python thread olarak calistirilan bot kodu.
// C++ thread'den degil, Python'un kendi threading modulu uzerinden calisir.
// Boylece game C extension'larini (player.GetStatus vb.) guvenli cagirabiliriz.
static const char* BOT_PYTHON_CODE = R"(
import threading as _t
import time as _time

try:
    import player as _player
    import net as _net
    import chr as _chr
    import chrmgr as _chrmgr
    _modules_ok = True
except ImportError:
    _modules_ok = False

_m2bot_running = True
_m2bot_tick    = 0.10   # saniye

def _m2bot_loop():
    global _m2bot_running
    _log_interval = 5.0
    _last_log = 0.0
    while _m2bot_running:
        try:
            if _modules_ok:
                hp     = _player.GetStatus(_player.HP)
                maxhp  = _player.GetStatus(_player.MAX_HP)
                sp     = _player.GetStatus(_player.SP)
                maxsp  = _player.GetStatus(_player.MAX_SP)
                if maxhp > 0:
                    now = _time.time()
                    if now - _last_log >= _log_interval:
                        _last_log = now
                        # HP/SP logla (console icin Python'dan dogrudan yazamazsin,
                        # ama degerleri okuyabiliyoruz — ileride UI'ya tasinabilir)
                        pass
                    # HP kritikse bekle
                    if float(hp) / maxhp < 0.10:
                        _time.sleep(2.0)
                        continue
        except Exception:
            pass
        _time.sleep(_m2bot_tick)

_m2bot_thread = _t.Thread(target=_m2bot_loop)
_m2bot_thread.daemon = True
_m2bot_thread.start()
)";

static const char* BOT_STOP_CODE =
    "_m2bot_running = False\n";

void Bot::Start() {
    if (m_running.exchange(true)) return;

    FileLog::Write("[Bot] Python thread baslatiliyor...");
    PyAPI::Exec(BOT_PYTHON_CODE);
    Console::Log("[Bot] Python thread baslatildi");
    FileLog::Write("[Bot] Python thread baslatildi");
}

void Bot::Stop() {
    if (!m_running.exchange(false)) return;

    PyAPI::Exec(BOT_STOP_CODE);
    Console::Log("[Bot] Durduruldu");
    FileLog::Write("[Bot] Durduruldu");
}

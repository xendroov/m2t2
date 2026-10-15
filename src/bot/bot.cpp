#include "bot.h"
#include "../game/pyapi.h"
#include "../core/console.h"
#include "../core/filelog.h"
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>

Bot& Bot::Instance() {
    static Bot inst;
    return inst;
}

// Bir isim ilginc mi? (oyun modullerini sys.modules yiginindan ayiklamak icin)
static bool Interesting(const std::string& n) {
    const char* keys[] = {
        "player", "net", "chr", "char", "item", "shop", "quest",
        "nonplayer", "background", "m2g", "m2net", "guild", "pack", nullptr
    };
    std::string low;
    for (char c : n) low += (char)tolower((unsigned char)c);
    for (int i = 0; keys[i]; ++i)
        if (low.find(keys[i]) != std::string::npos) return true;
    return false;
}

void Bot::Start() {
    if (m_running.exchange(true)) return;

    FileLog::Write("[Bot] === sys.modules DOKUMU ===");

    auto names = PyAPI::ModuleNames();
    FileLog::Write("[Bot] Toplam modul sayisi: %d", (int)names.size());

    // Ilginc modulleri (player/net/chr/... iceren) logla
    std::string line;
    int shown = 0;
    for (auto& n : names) {
        if (!Interesting(n)) continue;
        line += n;
        line += "  ";
        ++shown;
        if (line.size() > 180) {
            FileLog::Write("[Bot] ILGINC: %s", line.c_str());
            line.clear();
        }
    }
    if (!line.empty()) FileLog::Write("[Bot] ILGINC: %s", line.c_str());
    FileLog::Write("[Bot] Ilginc modul: %d adet", shown);

    // Bilinen isimleri __main__'e enjekte etmeyi dene (hem sade hem m2g versiyonlari)
    struct Pair { const char* sysName; const char* as; };
    Pair tryList[] = {
        {"player", "player"}, {"playerm2g2", "player"}, {"playerm2g", "player"},
        {"net", "net"},       {"m2netm2g", "net"},      {"netm2g", "net"},
        {"chr", "chr"},       {"chrm2g", "chr"},
        {"chrmgr", "chrmgr"}, {"chrmgrm2g", "chrmgr"},
        {"nonplayer", "nonplayer"}, {"nonplayerm2g", "nonplayer"},
    };
    for (auto& p : tryList) {
        if (PyAPI::InjectModule(p.sysName, p.as)) {
            FileLog::Write("[Bot] ENJEKTE: sys.modules['%s'] -> __main__.%s", p.sysName, p.as);
        }
    }

    FileLog::Write("[Bot] === ENJEKSIYON SONRASI TEST ===");
    FileLog::Write("[Bot] player        = %s", PyAPI::EvalStr("str(player)").c_str());
    FileLog::Write("[Bot] chrmgr         = %s", PyAPI::EvalStr("str(chrmgr)").c_str());
    FileLog::Write("[Bot] net            = %s", PyAPI::EvalStr("str(net)").c_str());
    FileLog::Write("[Bot] GetTargetVID() = %s", PyAPI::EvalStr("str(player.GetTargetVID())").c_str());
    FileLog::Write("[Bot] GetStatus(0)   = %s", PyAPI::EvalStr("str(player.GetStatus(0))").c_str());
    FileLog::Write("[Bot] === TEST BITTI ===");

    Console::Log("[Bot] Tani tamam — C:\\m2t2bot_log.txt dosyasini kontrol et");
    FileLog::Write("[Bot] Baslatildi (Tick pasif — once modul isimlerini gorelim)");
}

void Bot::Stop() {
    if (!m_running.exchange(false)) return;
    Console::Log("[Bot] Durduruldu");
    FileLog::Write("[Bot] Durduruldu");
}

void Bot::Tick() {
    // PASIF. sys.modules dokumunu gorene kadar surekli Python cagrisi yapmiyoruz.
    // Crash'in surekli tick'ten mi geldigini de boylece izole ediyoruz.
}

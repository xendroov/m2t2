#include "bot.h"
#include "../game/pyapi.h"
#include "../core/console.h"
#include "../core/filelog.h"
#include "../core/wndhook.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

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

// Oyun Python fonksiyonlarinin gercek C adreslerini ve kod baytlarini
// C:\m2t2dump.bin dosyasina yazar. VMProtect'te bile calisir (bellekteki
// acilmis kodu okur). Bu dosyayi analiz edip CPythonCharacterManager'i buluruz.
void Bot::DumpInfo() {
    HMODULE base = GetModuleHandleA(nullptr);
    FileLog::Write("[Dump] module base = 0x%p", (void*)base);

    // Modulleri garanti enjekte et
    PyAPI::InjectModule("playerm2g2", "player");
    PyAPI::InjectModule("m2netm2g",   "net");
    PyAPI::InjectModule("chrmgrm2g",  "chrmgr");
    PyAPI::InjectModule("chr",        "chr");
    PyAPI::InjectModule("nonplayer",  "nonplayer");

    static const char* exprs[] = {
        "player.GetStatus", "player.GetMainCharacterIndex", "player.GetTargetVID",
        "player.GetMainCharacterPosition", "player.SetTarget",
        "chrmgr.IsDead", "chrmgr.IsPC", "chrmgr.GetVIDInfo", "chrmgr.GetPickedVID",
        "chr.GetProjectPosition", "chr.GetNameByVID", "chr.SelectCharacter",
        "nonplayer.GetVnumByVID", "net.SendOnClickPacket", nullptr
    };

    FILE* f = nullptr;
    fopen_s(&f, "C:\\m2t2dump.bin", "wb");
    if (!f) { FileLog::Write("[Dump] C:\\m2t2dump.bin acilamadi"); return; }

    uint32_t magic = 0x3242324D; // 'M2B2'
    uint32_t mb = (uint32_t)(uintptr_t)base;
    fwrite(&magic, 4, 1, f);
    fwrite(&mb, 4, 1, f);

    // PE'den SizeOfImage
    uint8_t* p = reinterpret_cast<uint8_t*>(base);
    uint32_t e_lfanew   = *reinterpret_cast<uint32_t*>(p + 0x3C);
    uint32_t sizeOfImage= *reinterpret_cast<uint32_t*>(p + e_lfanew + 0x50);
    fwrite(&sizeOfImage, 4, 1, f);
    FileLog::Write("[Dump] SizeOfImage = 0x%X", sizeOfImage);

    for (int i = 0; exprs[i]; ++i) {
        uintptr_t addr = PyAPI::ResolveCFunc(exprs[i]);
        uint32_t rva = addr ? (uint32_t)(addr - (uintptr_t)base) : 0;
        FileLog::Write("[Dump] %-32s 0x%08X (RVA 0x%X)", exprs[i], (uint32_t)addr, rva);

        uint8_t nameLen = (uint8_t)strlen(exprs[i]);
        fwrite(&nameLen, 1, 1, f);
        fwrite(exprs[i], nameLen, 1, f);
        uint32_t a32 = (uint32_t)addr;
        fwrite(&a32, 4, 1, f);

        uint16_t codeLen = 1024;
        fwrite(&codeLen, 2, 1, f);
        if (addr) {
            fwrite(reinterpret_cast<void*>(addr), codeLen, 1, f);
        } else {
            uint8_t z[1024] = {0};
            fwrite(z, codeLen, 1, f);
        }
    }
    fclose(f);
    FileLog::Write("[Dump] C:\\m2t2dump.bin yazildi — bu dosyayi gonder");
    Console::Log("[Dump] C:\\m2t2dump.bin olusturuldu — Claude'a gonder");
}

// WndProc hook'tan her mesajda ANA THREAD'de cagrilir.
void Bot::OnFrame() {
    // Dump istegi (END tusu) — bot kapali olsa bile calissin
    if (m_dumpReq.exchange(false)) {
        DumpInfo();
    }

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
    int sp    = PyAPI::EvalInt("player.GetStatus(7)");   // SP
    int maxsp = PyAPI::EvalInt("player.GetStatus(8)");   // MAX_SP

    // 2 saniyede bir canli durum yaz (botun calistigini gorursun)
    if (now - m_lastStatus >= 2000) {
        m_lastStatus = now;
        int lvl  = PyAPI::EvalInt("player.GetStatus(1)");
        int tvid = PyAPI::EvalInt("player.GetTargetVID()");
        Console::Log("[Bot] Lv%d  HP %d/%d  SP %d/%d  hedef=%d",
                     lvl, hp, maxhp, sp, maxsp, tvid);
    }

    // --- Otomatik iksir (tus gonderimi — quickslot'a iksir koymalisin) ---
    if (m_autoPotion) {
        if (maxhp > 0 && (hp * 100 / maxhp) < m_hpPotPct && (now - m_lastHpPot) > 800) {
            WndHook::SendKey(m_hpPotKey);
            m_lastHpPot = now;
            FileLog::Write("[Bot] HP %%%d - iksir (F%d)", hp * 100 / maxhp, m_hpPotKey - 0x6F);
        }
        if (maxsp > 0 && (sp * 100 / maxsp) < m_spPotPct && (now - m_lastSpPot) > 800) {
            WndHook::SendKey(m_spPotKey);
            m_lastSpPot = now;
        }
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

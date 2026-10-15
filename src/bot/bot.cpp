#include "bot.h"
#include "../game/pyapi.h"
#include "../core/console.h"
#include "../core/filelog.h"
#include "../core/wndhook.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <tuple>
#include <vector>

// --- Dump v2 analizinden dogrulanan offsetler (RVA, module base'e gore) ---
//   CPythonCharacterManager pointer : *(base + 0x2EDD980)
//   alive instance map             : manager + 0x20  {head ptr, size}
//   RB-tree node: +0x00 left, +0x04 parent, +0x08 right, +0x0D isnil,
//                 +0x10 VID, +0x14 instance*
static const uintptr_t RVA_CHARMGR = 0x2EDD980;
static const uintptr_t MAP_OFF     = 0x20;

Bot& Bot::Instance() {
    static Bot inst;
    return inst;
}

// Metin2 POINT sabitleri — GetStatus taramasiyla DOGRULANDI (23:35 log):
//   1=LEVEL  3=EXP  4=NEXT_EXP  5=HP  6=MAX_HP  7=SP  8=MAX_SP

void Bot::Start() {
    if (m_running.exchange(true)) return;
    Console::Log("[Bot] Baslatildi — otomatik farm aktif (INSERT=dur)");
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
    Console::Log("[Bot] Moduller hazir — otomatik mob tarama aktif");
    FileLog::Write("[Bot] Setup tamam");
}

// SEH korumali bellek okuma — gecersiz adreste crash yerine false doner.
static bool DumpRead(uintptr_t src, void* dst, size_t n) {
    __try {
        memcpy(dst, reinterpret_cast<const void*>(src), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        memset(dst, 0, n);
        return false;
    }
}

static uintptr_t derefOr0(uintptr_t addr) {
    uintptr_t v = 0;
    DumpRead(addr, &v, 4);
    return v;
}

// Dosyaya bir ham bellek bolgesi yazar: nameLen, name, addr, len(u32), bytes.
static void DumpRegion(FILE* f, const char* name, uintptr_t addr, uint32_t len) {
    static uint8_t buf[0x1000];
    if (len > sizeof(buf)) len = sizeof(buf);
    uint8_t nameLen = (uint8_t)strlen(name);
    fwrite(&nameLen, 1, 1, f);
    fwrite(name, nameLen, 1, f);
    uint32_t a32 = (uint32_t)addr;
    fwrite(&a32, 4, 1, f);
    fwrite(&len, 4, 1, f);
    if (addr) DumpRead(addr, buf, len); else memset(buf, 0, len);
    fwrite(buf, len, 1, f);
    FileLog::Write("[Dump] bolge %s @ 0x%08X (%u bayt)", name, a32, len);
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

    // --- Bolum B: ham bellek bolgeleri (karakter yoneticisi zinciri) ---
    // Marker: bir bos isim (nameLen=0) bolum B'nin baslangicini isaretler
    uint8_t zero = 0; fwrite(&zero, 1, 1, f);

    uintptr_t ub = (uintptr_t)base;
    uintptr_t mgr = 0, vtable = 0, getInst = 0;
    DumpRead(ub + 0x2EDD980, &mgr, 4);
    if (mgr) DumpRead(mgr + 4, &vtable, 4);
    if (vtable) DumpRead(vtable + 8, &getInst, 4);
    FileLog::Write("[Dump] mgr=0x%08X vtable=0x%08X GetInstancePtr=0x%08X",
                   (uint32_t)mgr, (uint32_t)vtable, (uint32_t)getInst);

    DumpRegion(f, "mgr_obj",        mgr,                 0x400);
    DumpRegion(f, "vtable",         vtable,              0x100);
    DumpRegion(f, "GetInstancePtr", getInst,             0x800);
    DumpRegion(f, "GetPickedActor", ub + 0x2543310,      0x400);
    DumpRegion(f, "player_obj",     derefOr0(ub + 0x2EDD9E4), 0x200);

    fclose(f);
    FileLog::Write("[Dump] C:\\m2t2dump.bin yazildi — bu dosyayi gonder");
    Console::Log("[Dump] C:\\m2t2dump.bin olusturuldu — Claude'a gonder");
}

// Alive instance map'i (RB-tree) gezip tum VID'leri toplar.
// ANA THREAD'de cagrilmali; DumpRead SEH korumali (bozuk pointer'da crash yok).
std::vector<uint32_t> Bot::EnumerateVIDs() {
    std::vector<uint32_t> vids;
    uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
    uint32_t mgr = 0;
    DumpRead(base + RVA_CHARMGR, &mgr, 4);
    if (!mgr) return vids;

    uint32_t head = 0, size = 0;
    DumpRead(mgr + MAP_OFF,     &head, 4);
    DumpRead(mgr + MAP_OFF + 4, &size, 4);
    if (!head || size == 0 || size > 8000) return vids;

    uint32_t root = 0;
    DumpRead(head + 4, &root, 4);  // head->_Parent = root

    // Iteratif DFS — nil (isnil) ve head'de dur
    static uint32_t stack[2048];
    int sp = 0, guard = 0;
    if (root && root != head) stack[sp++] = root;
    while (sp > 0 && guard++ < 16000) {
        uint32_t n = stack[--sp];
        if (!n || n == head) continue;
        uint8_t isnil = 1;
        DumpRead(n + 0x0D, &isnil, 1);
        if (isnil) continue;

        uint32_t vid = 0;
        DumpRead(n + 0x10, &vid, 4);
        if (vid) vids.push_back(vid);

        uint32_t l = 0, r = 0;
        DumpRead(n + 0x00, &l, 4);
        DumpRead(n + 0x08, &r, 4);
        if (sp < 2046) {
            if (l && l != head) stack[sp++] = l;
            if (r && r != head) stack[sp++] = r;
        }
        if ((int)vids.size() > 4000) break;
    }
    return vids;
}

// Yakindaki en yakin uygun mob'u bulup m_targetVid'e yazar.
void Bot::ScanTarget() {
    std::vector<uint32_t> vids = EnumerateVIDs();
    if (vids.empty()) { m_targetVid = 0; return; }

    uint32_t myVid = (uint32_t)PyAPI::EvalInt("player.GetMainCharacterIndex()");
    auto mypos = PyAPI::EvalXYZ("player.GetMainCharacterPosition()");
    float mx = std::get<0>(mypos), my = std::get<1>(mypos);

    uint32_t best = 0; float bestD2 = m_farmRange * m_farmRange;
    char buf[96];
    for (uint32_t vid : vids) {
        if (vid == myVid || vid == 0) continue;

        snprintf(buf, sizeof(buf), "nonplayer.GetVnumByVID(%u)", vid);
        if (PyAPI::EvalInt(buf) <= 0) continue;        // oyuncu/gecersiz — atla

        snprintf(buf, sizeof(buf), "chrmgr.IsDead(%u)", vid);
        if (PyAPI::EvalBool(buf)) continue;            // olu — atla

        snprintf(buf, sizeof(buf), "chr.IsNPC(%u)", vid);
        if (PyAPI::EvalBool(buf)) continue;            // NPC — atla
        snprintf(buf, sizeof(buf), "chr.IsShop(%u)", vid);
        if (PyAPI::EvalBool(buf)) continue;            // dukkanci — atla

        if (m_targetName[0]) {                          // isim filtresi
            snprintf(buf, sizeof(buf), "chr.GetNameByVID(%u)", vid);
            std::string nm = PyAPI::EvalStr(buf);
            if (nm.find(m_targetName) == std::string::npos) continue;
        }

        snprintf(buf, sizeof(buf), "chr.GetProjectPosition(%u)", vid);
        auto p = PyAPI::EvalXYZ(buf);
        float dx = std::get<0>(p) - mx, dy = std::get<1>(p) - my;
        float d2 = dx*dx + dy*dy;
        if (d2 < bestD2) { bestD2 = d2; best = vid; }
    }

    if (best && best != m_targetVid) {
        snprintf(buf, sizeof(buf), "chr.GetNameByVID(%u)", best);
        std::string nm = PyAPI::EvalStr(buf);
        Console::Log("[Bot] Hedef: %s (VID %u) ~%.0f", nm.c_str(), best, sqrtf(bestD2));
        snprintf(buf, sizeof(buf), "player.SetTarget(%u)", best);
        PyAPI::Exec(buf);
    }
    m_targetVid = best;
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
        Console::Log("[Bot] Lv%d  HP %d/%d  SP %d/%d  hedef=%u",
                     lvl, hp, maxhp, sp, maxsp, m_targetVid);
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

    char buf[96];

    if (m_autoFarm) {
        // Periyodik tarama: en yakin mob'u hedefle
        if (now - m_lastScan >= 600) {
            m_lastScan = now;
            // mevcut hedef oldu/kayboldu mu?
            if (m_targetVid) {
                snprintf(buf, sizeof(buf), "chrmgr.IsDead(%u)", m_targetVid);
                if (PyAPI::EvalBool(buf)) m_targetVid = 0;
            }
            ScanTarget();
        }
        // Hedefe saldir
        if (m_targetVid) {
            snprintf(buf, sizeof(buf), "chrmgr.IsDead(%u)", m_targetVid);
            if (PyAPI::EvalBool(buf)) { m_targetVid = 0; return; }
            snprintf(buf, sizeof(buf), "net.SendOnClickPacket(%u)", m_targetVid);
            PyAPI::Exec(buf);
        }
    } else {
        // Manuel mod: oyuncunun secili hedefine vur
        int vid = PyAPI::EvalInt("player.GetTargetVID()");
        if (vid <= 0) return;
        snprintf(buf, sizeof(buf), "chrmgr.IsDead(%d)", vid);
        if (PyAPI::EvalBool(buf)) return;
        snprintf(buf, sizeof(buf), "net.SendOnClickPacket(%d)", vid);
        PyAPI::Exec(buf);
    }
}

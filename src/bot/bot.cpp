#include "bot.h"
#include "../game/pyapi.h"
#include "../core/console.h"
#include "../core/filelog.h"
#include "../core/wndhook.h"
#include "../core/sigscan.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <tuple>
#include <vector>
#include <algorithm>

// --- Dump v2 analizinden dogrulanan offsetler (RVA, module base'e gore) ---
//   CPythonCharacterManager pointer : *(base + 0x2EDD980)
//   alive instance map             : manager + 0x20  {head ptr, size}
//   RB-tree node: +0x00 left, +0x04 parent, +0x08 right, +0x0D isnil,
//                 +0x10 VID, +0x14 instance*
static const uintptr_t RVA_CHARMGR   = 0x2EDD980;
static const uintptr_t MAP_OFF       = 0x20;
// CPythonNetworkStream singleton: netstream = *(base + 0x2EDD794)
static const uintptr_t RVA_NETSTREAM = 0x2EDD794;
// SendAttackPacket ARTIK SABIT DEGIL — calisma-zamani imza taramasiyla bulunur
// (bkz. ResolveAttack). Onceki sabit 0x2663FC0 YANLISTI: o SendQuestConfirmPacket'ti,
// bu yuzden "gonderildi ama hasar yok" oluyordu. Dogru fonksiyon
// "Send Battle Attack Packet Error" debug string'ini referans eder.
// Dogrulanan komsu primitifler (dump6 analizi, bu client surumu icin yedek):
// Send'in hemen ardinda SendSequence (0x2793D20) gelir; 0x200'luk dump ikisini de kapsar.
static const uintptr_t RVA_SEND_FALLBACK = 0x2793D10; // CPythonNetworkStream::Send(size,buf)

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

    // Saldiri fonksiyonunu canli bellekte coz (bir kez). m_attack_ready'yi ayarlar.
    ResolveAttack();
    if (!m_attack_ready)
        Console::Log("[Bot] UYARI: SendAttackPacket cozulemedi — END ile dump al, bana gonder");
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

// =====================================================================
//  SALDIRI FONKSIYONU COZUMLEME (runtime imza taramasi)
// ---------------------------------------------------------------------
//  Onceki surumde SendAttackPacket adresi SABIT (0x2663FC0) idi ama o adres
//  aslinda SendQuestConfirmPacket'ti -> paket gidiyordu, hasar yoktu.
//  Dogru fonksiyon tek bir string'i referans eder: "Send Battle Attack
//  Packet Error". O string'i canli bellekte bulup, onu push eden komutu
//  (68 xx xx xx xx) buluyoruz; oradan fonksiyon basina (55 8B EC) geri
//  yuruyoruz. Boylece client surumunden BAGIMSIZ, kendi kendini ayarlar.
//  Fonksiyon govdesinden paket header'ini (0x52 bekleniyor), arg sayisini
//  (ret N) ve VID'in hangi argumanda oldugunu (dword store) okuruz.
// =====================================================================
struct AttackResolve {
    uintptr_t fn        = 0;   // SendAttackPacket adresi
    uintptr_t sendFn    = 0;   // CPythonNetworkStream::Send(size, buf)
    uint8_t   header    = 0;   // paket ilk baytı (0x52)
    int       argBytes  = -1;  // ret N  (4 => 1 arg, 8 => 2 arg)
    int       vidSlot   = 0;   // 8 veya 12: VID hangi [ebp+X]'te (dword)
    int       pktSize   = 0;   // Send'e verilen boyut (push <size>)
    uint32_t  strAddr   = 0;   // bulunan hata string'i
    uint32_t  refAddr   = 0;   // push <str> komutu
    bool      ok         = false;
};
static AttackResolve g_atk;

// thiscall -> __fastcall(this, edx-dummy, ...stack args)
typedef char(__fastcall* tThis1)(void*, int, uint32_t);
typedef char(__fastcall* tThis2)(void*, int, uint32_t, uint32_t);

// Fonksiyon govdesini (SEH korumali) yerel tampona kopyala.
static size_t CopyCode(uintptr_t fn, uint8_t* out, size_t cap) {
    size_t n = 0;
    for (; n < cap; ++n) if (!DumpRead(fn + n, &out[n], 1)) break;
    return n;
}

// code icinde pat'i ara; bulursa ofsetini, yoksa -1.
static int FindPat(const uint8_t* code, size_t len, const uint8_t* pat, size_t pl, int from=0) {
    if (len < pl) return -1;
    for (int i = from; i <= (int)(len - pl); ++i) {
        size_t j = 0; for (; j < pl; ++j) if (code[i+j] != pat[j]) break;
        if (j == pl) return i;
    }
    return -1;
}

// SendAttackPacket'i bul ve govdesini coz. Ana thread'de bir kez cagrilir.
void Bot::ResolveAttack() {
    uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
    g_atk = AttackResolve{};
    m_attack_ready = false;

    FileLog::Write("[RESOLVE] SendAttackPacket aranıyor (imza taramasi)...");
    Console::Log("[RESOLVE] Saldiri fonksiyonu araniyor...");

    // 1) Hata string'ini bul
    uintptr_t s = SigScan::FindString("Send Battle Attack Packet Error");
    if (!s) {
        FileLog::Write("[RESOLVE] HATA: 'Send Battle Attack Packet Error' string'i bellekte YOK");
        Console::Log("[RESOLVE] BASARISIZ: string bulunamadi (yedek Send kullanilacak)");
        return;
    }
    g_atk.strAddr = (uint32_t)s;
    FileLog::Write("[RESOLVE] string @0x%08X (RVA 0x%X)", (uint32_t)s, (uint32_t)(s - base));

    // 2) push <str> referansini bul (68 xx xx xx xx), calistirilabilir bellekte
    uint8_t needle[5] = {0x68, (uint8_t)s, (uint8_t)(s>>8), (uint8_t)(s>>16), (uint8_t)(s>>24)};
    uintptr_t ref = SigScan::FindBytes(needle, 5, true);
    if (!ref) {  // bazen mov ile yuklenir -> ham 4 bayt ara
        ref = SigScan::FindBytes(needle + 1, 4, true);
        if (ref) ref -= 1; // push baytina hizala (yaklasik)
    }
    if (!ref) {
        FileLog::Write("[RESOLVE] HATA: string'e referans (push) bulunamadi");
        Console::Log("[RESOLVE] BASARISIZ: referans yok");
        return;
    }
    g_atk.refAddr = (uint32_t)ref;
    FileLog::Write("[RESOLVE] push-ref @0x%08X (RVA 0x%X)", (uint32_t)ref, (uint32_t)(ref - base));

    // 3) Fonksiyon basina geri yuru
    uintptr_t fn = SigScan::FindFunctionStart(ref, 0x800);
    if (!fn) {
        FileLog::Write("[RESOLVE] HATA: fonksiyon prologu (55 8B EC) bulunamadi");
        Console::Log("[RESOLVE] BASARISIZ: fonksiyon basi yok");
        return;
    }
    g_atk.fn = fn;
    FileLog::Write("[RESOLVE] SendAttackPacket @0x%08X (RVA 0x%X)", (uint32_t)fn, (uint32_t)(fn - base));

    // 4) Govdeyi coz
    uint8_t code[0x260];
    size_t clen = CopyCode(fn, code, sizeof(code));

    // 4a) header: prologdan sonraki ilk "C6 45 xx HH" (mov byte [ebp+d8], imm8)
    uint8_t hp[2] = {0xC6, 0x45};
    int hi = FindPat(code, clen, hp, 2);
    if (hi >= 0 && hi + 3 < (int)clen) g_atk.header = code[hi + 3];

    // 4b) ret N -> C2 NN 00  (NN = temizlenecek arg bayti: 4=1 arg, 8=2 arg)
    for (int i = 0; i + 2 < (int)clen; ++i)
        if (code[i]==0xC2 && code[i+2]==0x00 &&
            (code[i+1]==0x04 || code[i+1]==0x08 || code[i+1]==0x0C)) {
            g_atk.argBytes = code[i+1]; break;
        }
    // Bulunamazsa 2-arg varsay (standart SendAttackPacket(bType,vid) = ret 8).
    // YANLIS arg sayisi ESP bozar; bu yuzden {4,8} disi degeri 8'e sabitliyoruz.
    if (g_atk.argBytes != 4 && g_atk.argBytes != 8) g_atk.argBytes = 8;

    // 4c) push <str>'in kod-icindeki ofseti -> hemen onundeki E8 = Send
    int strOff = FindPat(code, clen, needle, 5);
    if (strOff > 0) {
        int lastCall = -1, lastSize = -1;
        for (int i = 0; i < strOff; ++i) {
            if (code[i]==0xE8) lastCall = i;                 // call rel32
            if (code[i]==0x6A) lastSize = code[i+1];          // push imm8 (boyut)
        }
        if (lastCall >= 0) {
            int32_t rel = *(int32_t*)(code + lastCall + 1);
            g_atk.sendFn = fn + lastCall + 5 + rel;           // E8 hedefi
        }
        if (lastSize >= 0) g_atk.pktSize = lastSize;
    }

    // 4d) VID = pakete DWORD olarak yazilan arg. Standart imza
    //     SendAttackPacket(UINT uMotAttack, DWORD dwVID)'da HER IKI arg de 32-bit,
    //     yani ikisi de "8B /r [ebp+slot]" ile okunur. Ayrim pakete YAZILMA
    //     seklinde: bType -> BYTE store (88), VID -> DWORD store (89).
    //     Bir arg yuklemesini (8B /r [ebp+8|C]) takip eden ilk 88 = bType (atla),
    //     ilk 89 (mem) = VID.
    for (int i = 0; i + 2 < (int)clen; ++i) {
        if (code[i] != 0x8B || (code[i+1] & 0xC7) != 0x45) continue;
        uint8_t slot = code[i+2];
        if (slot != 0x08 && slot != 0x0C) continue;
        for (int k = i + 3; k < i + 14 && k + 1 < (int)clen; ++k) {
            if (code[k] == 0x88) break;                         // byte store => bType
            if (code[k] == 0x89 && (code[k+1] & 0xC0) != 0xC0) { // dword mem store => VID
                g_atk.vidSlot = slot; break;
            }
        }
        if (g_atk.vidSlot) break;
    }
    if (!g_atk.vidSlot) {
        // tek arg (ret 4) => VID [ebp+8]; iki arg => standart (bType@8, vid@C)
        g_atk.vidSlot = (g_atk.argBytes == 4) ? 0x08 : 0x0C;
    }

    g_atk.ok = true;
    m_attack_ready = true;
    FileLog::Write("[RESOLVE] header=0x%02X  ret=%d (arg=%d)  vid=[ebp+0x%X]  pktSize=%d  Send@0x%08X",
                   g_atk.header, g_atk.argBytes, g_atk.argBytes>=0?g_atk.argBytes/4:-1,
                   g_atk.vidSlot, g_atk.pktSize, (uint32_t)g_atk.sendFn);
    Console::Log("[RESOLVE] OK  SendAttackPacket=0x%08X header=0x%02X vidArg=+0x%X size=%d",
                 (uint32_t)fn, g_atk.header, g_atk.vidSlot, g_atk.pktSize);
}

// SendAttackPacket'i cozulmus cagri geleneğiyle cagir.
// Donus: 1=gonderildi, 0=oyun reddetti (precheck/Send false), -1=hata/hazir degil.
static int GameAttack(uint32_t vid, int bType) {
    if (!vid || !g_atk.ok || !g_atk.fn) return -1;
    uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
    uint32_t netstream = 0;
    DumpRead(base + RVA_NETSTREAM, &netstream, 4);
    if (!netstream) return -1;
    void* self = reinterpret_cast<void*>(netstream);
    int ret = -1;
    __try {
        if (g_atk.argBytes == 4) {
            // tek arg: SendAttackPacket(vid)
            ret = (int)(unsigned char)((tThis1)g_atk.fn)(self, 0, vid);
        } else {
            // iki arg: VID + type. vidSlot hangi slota denk geliyorsa oraya VID.
            uint32_t a = (g_atk.vidSlot == 8) ? vid : (uint32_t)bType; // [ebp+8]
            uint32_t b = (g_atk.vidSlot == 8) ? (uint32_t)bType : vid; // [ebp+0xC]
            ret = (int)(unsigned char)((tThis2)g_atk.fn)(self, 0, a, b);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { ret = -1; }
    return ret;
}

// Dosyaya bir ham bellek bolgesi yazar: nameLen, name, addr, len(u32), bytes.
static void DumpRegion(FILE* f, const char* name, uintptr_t addr, uint32_t len) {
    static uint8_t buf[0x8000];
    if (len > sizeof(buf)) len = sizeof(buf);
    uint8_t nameLen = (uint8_t)strlen(name);
    fwrite(&nameLen, 1, 1, f);
    fwrite(name, nameLen, 1, f);
    uint32_t a32 = (uint32_t)addr;
    fwrite(&a32, 4, 1, f);
    fwrite(&len, 4, 1, f);
    memset(buf, 0, len);
    // Sayfa sayfa oku — bir sayfa hatali olsa digerleri kurtulur
    if (addr) {
        for (uint32_t off = 0; off < len; off += 0x1000) {
            uint32_t chunk = (len - off < 0x1000) ? (len - off) : 0x1000;
            DumpRead(addr + off, buf + off, chunk);
        }
    }
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

    // Modullerin TUM fonksiyonlarini listele — dogru saldiri fonksiyonunu bulmak icin
    FileLog::Write("[dir] net    = %s", PyAPI::EvalStr("' '.join(dir(net))").c_str());
    FileLog::Write("[dir] player = %s", PyAPI::EvalStr("' '.join(dir(player))").c_str());
    FileLog::Write("[dir] chr    = %s", PyAPI::EvalStr("' '.join(dir(chr))").c_str());
    FileLog::Write("[dir] chrmgr = %s", PyAPI::EvalStr("' '.join(dir(chrmgr))").c_str());

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

    // CPythonNetworkStream: singleton *(base+0x2EDD794), ic SendOnClickPacket
    // base+0x2661BD0. SendAttackPacket bu metodun komsusu — genis bolge cek.
    uintptr_t netstream = derefOr0(ub + 0x2EDD794);
    FileLog::Write("[Dump] netstream=0x%08X", (uint32_t)netstream);
    DumpRegion(f, "netstream_obj",  netstream,           0x200);
    DumpRegion(f, "net_code",       ub + 0x2661BD0 - 0x3000, 0x8000);
    // .rdata debug string'leri — saldiri fonksiyonunu dogrulamak icin
    // (0x2fdd97c onclick, 0x2fddfac/0x2fddfcc aday2, 0x2fde2e4 aday3 civari)
    DumpRegion(f, "strings",        ub + 0x297D800,      0x1200);

    // --- COZULEN SendAttackPacket: imza taramasini burada da calistir,
    //     bulunan fonksiyonun govdesini dump'a yaz (offline dogrulama icin) ---
    ResolveAttack();
    if (g_atk.fn) {
        FileLog::Write("[Dump] SendAttackPacket @0x%08X header=0x%02X ret=%d vid=+0x%X Send@0x%08X",
                       (uint32_t)g_atk.fn, g_atk.header, g_atk.argBytes, g_atk.vidSlot, (uint32_t)g_atk.sendFn);
        DumpRegion(f, "SendAttackPacket", g_atk.fn,     0x300);
    } else {
        FileLog::Write("[Dump] SendAttackPacket COZULEMEDI (string/ref/prolog bulunamadi)");
    }
    if (g_atk.sendFn) DumpRegion(f, "Send_fn",     g_atk.sendFn, 0x200);
    // Dogrulanan yedek Send/Seq adreslerini de cek (karsilastirma)
    DumpRegion(f, "Send_fallback", ub + RVA_SEND_FALLBACK, 0x200);

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
    static DWORD lastDbg = 0;
    DWORD nowDbg = GetTickCount();
    bool dbg = (nowDbg - lastDbg) >= 3000;  // 3 saniyede bir detayli log
    if (dbg) { lastDbg = nowDbg; FileLog::Write("[Scan] enumere VID sayisi=%d", (int)vids.size()); }
    if (vids.empty()) { m_targetVid = 0; return; }

    uint32_t myVid = (uint32_t)PyAPI::EvalInt("player.GetMainCharacterIndex()");

    uint32_t best = 0; float bestDist = 1e30f;   // en yakini sec
    int cand = 0, sample = 0;
    char buf[96];
    for (uint32_t vid : vids) {
        if (vid == myVid || vid == 0) continue;

        snprintf(buf, sizeof(buf), "nonplayer.GetVnumByVID(%u)", vid);
        if (PyAPI::EvalInt(buf) <= 0) continue;        // oyuncu/gecersiz — atla
        ++cand;

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

        // Mesafe: player.GetCharacterDistance(vid) — koordinat derdi yok
        snprintf(buf, sizeof(buf), "player.GetCharacterDistance(%u)", vid);
        float dist = PyAPI::EvalFloat(buf);
        if (dbg && sample < 3) {
            FileLog::Write("[Scan] ornek VID=%u mesafe=%.1f", vid, dist);
            ++sample;
        }
        if (dist >= 0.f && dist < bestDist) { bestDist = dist; best = vid; }
    }

    if (dbg) FileLog::Write("[Scan] mob aday=%d  secilen VID=%u  mesafe=%.1f  myVID=%u",
                            cand, best, best ? bestDist : -1.f, myVid);

    if (best && best != m_targetVid) {
        snprintf(buf, sizeof(buf), "chr.GetNameByVID(%u)", best);
        std::string nm = PyAPI::EvalStr(buf);
        Console::Log("[Bot] Hedef: %s (VID %u) ~%.1f", nm.c_str(), best, bestDist);
        FileLog::Write("[Bot] Hedef: %s (VID %u) ~%.1f", nm.c_str(), best, bestDist);
        snprintf(buf, sizeof(buf), "player.SetTarget(%u)", best);
        PyAPI::Exec(buf);
    }
    m_targetVid = best;
}

// Menzildeki TUM mob'lara hizlica SendAttackPacket gonderir (alan saldirisi,
// animasyonsuz). Tek hedef secmez — referanstaki "range attack" mantigi.
// "Zincir" mantigi: en yakindan basla, max N mob vur (rate-limit).
void Bot::AttackArea() {
    if (!g_atk.ok) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            FileLog::Write("[FARM] *** SALDIRI HAZIR DEGIL: SendAttackPacket cozulemedi. "
                           "Hic paket gonderilemez. END ile dump alip gonder. ***");
            Console::Log("[FARM] Saldiri fonksiyonu COZULEMEDI — END ile dump al");
        }
        return;
    }

    uint32_t myVid = (uint32_t)PyAPI::EvalInt("player.GetMainCharacterIndex()");
    ++m_farmPass;

    DWORD now = GetTickCount();
    static DWORD lastFull = 0;
    bool full = m_verbose && (now - lastFull >= 2000);  // ~2sn'de bir tam detay
    if (full) lastFull = now;

    // Oyuncu daha dunyaya tam girmediyse entity listesi oturmamistir ->
    // cop VID'ler (float bit-desenleri, bos isim, mesafe 0) cikar = "afallama".
    // Oyuncu hazir olana kadar farm yapma.
    if (myVid == 0) {
        if (full) FileLog::Write("[FARM] oyuncu hazir degil (myVID=0) — bekleniyor");
        return;
    }

    std::vector<uint32_t> vids = EnumerateVIDs();

    // 1) Adaylari topla; her eleme sebebini say (DEBUG: ne engelledi?)
    struct Cand { uint32_t vid; float d; };
    std::vector<Cand> cands;
    int nEnum = (int)vids.size();
    int nSelf = 0, nFar = 0, nInvalid = 0, nDead = 0, nNpc = 0, nShop = 0, nName = 0;
    char buf[96];
    for (uint32_t vid : vids) {
        if (vid == 0 || vid == myVid) { ++nSelf; continue; }
        snprintf(buf, sizeof(buf), "player.GetCharacterDistance(%u)", vid);
        float d = PyAPI::EvalFloat(buf);
        // d<=0: gecersiz/cop VID (gercek mob hic mesafe 0 vermez; en yakin
        // gercek mob bile ~100+ birim). Bu, "afallama" cop girdilerini eler.
        if (d <= 0.f || d > m_farmRange) { ++nFar; continue; }
        snprintf(buf, sizeof(buf), "nonplayer.GetVnumByVID(%u)", vid);
        if (PyAPI::EvalInt(buf) <= 0) { ++nInvalid; continue; }   // oyuncu/gecersiz
        snprintf(buf, sizeof(buf), "chrmgr.IsDead(%u)", vid);
        if (PyAPI::EvalBool(buf)) { ++nDead; continue; }          // olu
        snprintf(buf, sizeof(buf), "chr.IsNPC(%u)", vid);
        if (PyAPI::EvalBool(buf)) { ++nNpc; continue; }           // NPC
        snprintf(buf, sizeof(buf), "chr.IsShop(%u)", vid);
        if (PyAPI::EvalBool(buf)) { ++nShop; continue; }          // dukkanci
        if (m_targetName[0]) {
            snprintf(buf, sizeof(buf), "chr.GetNameByVID(%u)", vid);
            if (PyAPI::EvalStr(buf).find(m_targetName) == std::string::npos) { ++nName; continue; }
        }
        cands.push_back({ vid, d });
    }

    // 2) En yakindan uzaga sirala, max N vur
    std::sort(cands.begin(), cands.end(),
              [](const Cand& a, const Cand& b) { return a.d < b.d; });

    int attacked = 0, sent = 0, rej = 0, err = 0;
    for (auto& c : cands) {
        if (attacked >= m_maxTargets) break;
        int r = GameAttack(c.vid, m_attackType);
        ++attacked;
        if (r == 1) ++sent; else if (r == 0) ++rej; else ++err;
        if (full) {
            snprintf(buf, sizeof(buf), "chr.GetNameByVID(%u)", c.vid);
            std::string nm = PyAPI::EvalStr(buf);
            FileLog::Write("[FARM]   VID %-6u '%s' mesafe=%-5.0f -> SendAttack=%d %s",
                           c.vid, nm.c_str(), c.d, r,
                           r == 1 ? "(GONDERILDI)" : r == 0 ? "(OYUN REDDETTI)" : "(HATA)");
        }
    }

    if (full) {
        Console::Log("[FARM] menzilde=%d  gonderilen=%d  reddedilen=%d  hata=%d  (enum=%d)",
                     (int)cands.size(), sent, rej, err, nEnum);
        FileLog::Write("[FARM] pass#%u enum=%d menzilde=%d vuruldu=%d | gonderilen=%d reddedilen=%d hata=%d",
                       m_farmPass, nEnum, (int)cands.size(), attacked, sent, rej, err);
        FileLog::Write("[FARM]   elendi: ben/0=%d uzak=%d gecersiz=%d olu=%d NPC=%d dukkan=%d isim=%d",
                       nSelf, nFar, nInvalid, nDead, nNpc, nShop, nName);
        if (!cands.empty() && sent == 0)
            FileLog::Write("[FARM]   >> DIKKAT: menzilde mob var ama HIC paket gitmedi. "
                           "donus 0 => oyun precheck reddetti (state/hareket), -1 => fn/netstream yok.");
        else if (cands.empty() && nFar > 0)
            FileLog::Write("[FARM]   >> Menzilde mob yok; en yakinlar 'uzak' sayildi. "
                           "m_farmRange=%.0f yeterli mi?", m_farmRange);
    }
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
        // Alan saldirisi: menzildeki tum mob'lara hizlica saldir
        // (forum: 180-220ms araligi — rate-limit asma)
        if (now - m_lastScan >= (DWORD)m_attackDelayMs) {
            m_lastScan = now;
            AttackArea();
        }
    } else {
        // Manuel mod: oyuncunun secili hedefine vur
        int vid = PyAPI::EvalInt("player.GetTargetVID()");
        if (vid <= 0) return;
        snprintf(buf, sizeof(buf), "chrmgr.IsDead(%d)", vid);
        if (PyAPI::EvalBool(buf)) return;
        int r = GameAttack((uint32_t)vid, m_attackType);
        if (m_verbose)
            FileLog::Write("[FARM-manuel] hedef VID=%d SendAttack=%d", vid, r);
    }
}

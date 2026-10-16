#pragma once
// =====================================================================
//  M2T2 — DLL (oyun ici) ile harici ayar penceresi (m2t2gui.exe) arasinda
//  PAYLASIMLI HAFIZA sozlesmesi. Tek kaynak: her iki taraf da bunu include eder.
//
//  Kural: sadece sabit-genislikli tipler (int32/uint32/float/char[]) + pack(4).
//  Boylece 32-bit DLL ile (32/64-bit) GUI AYNI yerlesimi gorur. POINTER YOK.
// =====================================================================
#include <Windows.h>
#include <cstdint>
#include <cstring>

#define M2T2_SHM_NAME    "m2t2_shared_v1"   // session-local isimli mapping
#define M2T2_SHM_MAGIC   0x4D325432u         // 'M2T2'
#define M2T2_SHM_VERSION 1u

#pragma pack(push, 4)
struct M2T2Shared {
    uint32_t magic;          // M2T2_SHM_MAGIC — gecerlilik
    uint32_t version;        // M2T2_SHM_VERSION — yerlesim surumu
    uint32_t dllHeartbeat;   // DLL her frame artirir (GUI "bagli mi" anlar)
    uint32_t guiHeartbeat;   // GUI her yenilemede artirir

    // --- AYARLAR (GUI yazar, DLL okur) ---
    int32_t  botEnabled;     // 0/1 — bot acik mi (INSERT veya GUI)
    int32_t  autoFarm;       // 1 = range-saldiri modu, 0 = target modu
    int32_t  autoPotion;     // 0/1
    int32_t  verbose;        // 0/1 — ayrintili debug log
    float    farmRange;      // menzil
    int32_t  attackDelayMs;  // saldiri araligi (ms)
    int32_t  maxTargets;     // ayni anda max mob
    int32_t  attackType;     // SendAttackPacket bType
    int32_t  hpPotPct;       // HP bu %'nin altinda iksir
    int32_t  spPotPct;       // SP bu %'nin altinda iksir
    int32_t  hpPotKey;       // VK kodu (F1=0x70)
    int32_t  spPotKey;       // VK kodu (F2=0x71)
    int32_t  speedHackOn;    // 0/1 hareket hizi hilesi (DLL'de hedef bulununca aktif)
    int32_t  attackSpeedPct; // % (yakinda)
    int32_t  moveSpeedPct;   // % hareket hizi carpani
    int32_t  wallHackOn;     // 0/1 collision kapat (DLL'de hedef bulununca aktif)
    char     targetName[64]; // mob isim filtresi (bos = hepsi)

    // --- DURUM (DLL yazar, GUI okur) ---
    int32_t  attackReady;    // SendAttackPacket cozuldu mu
    int32_t  fastFilter;     // hizli filtre aktif mi
    int32_t  curHp, curMaxHp, curSp, curMaxSp, curLevel;
    int32_t  lastEnum;       // son taramada enumere mob
    int32_t  lastInRange;    // menzilde
    int32_t  lastSent;       // son taramada gonderilen paket
    uint32_t attackFnRva;    // cozulen SendAttackPacket RVA
    uint32_t playerVid;      // ana karakter VID

    int32_t  reserved[15];   // ileride alan eklemek icin (yerlesim bozulmaz)
};
#pragma pack(pop)

// Mantikli varsayilanlar (bot.h ile ayni).
static inline void M2T2_InitDefaults(M2T2Shared* s) {
    memset(s, 0, sizeof(*s));
    s->magic          = M2T2_SHM_MAGIC;
    s->version        = M2T2_SHM_VERSION;
    s->botEnabled     = 0;
    s->autoFarm       = 1;
    s->autoPotion     = 1;
    s->verbose        = 1;
    s->farmRange      = 2000.0f;
    s->attackDelayMs  = 150;
    s->maxTargets     = 7;
    s->attackType     = 0;
    s->hpPotPct       = 50;
    s->spPotPct       = 30;
    s->hpPotKey       = 0x70;  // F1
    s->spPotKey       = 0x71;  // F2
    s->speedHackOn    = 0;
    s->attackSpeedPct = 100;
    s->moveSpeedPct   = 150;
    s->wallHackOn     = 0;
    s->targetName[0]  = 0;
}

// Mapping'i olustur/ac. create=true: yoksa olusturur (ilk acan). outHandle ve
// created opsiyonel. Basarisizsa nullptr. created=true => bu cagri olusturdu
// (varsayilanlari yazisin diye).
static inline M2T2Shared* M2T2_OpenShared(bool create, HANDLE* outHandle, bool* created) {
    if (created) *created = false;
    if (outHandle) *outHandle = nullptr;
    HANDLE h = nullptr;
    if (create) {
        h = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                               0, sizeof(M2T2Shared), M2T2_SHM_NAME);
        if (h && GetLastError() != ERROR_ALREADY_EXISTS && created) *created = true;
    } else {
        h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, M2T2_SHM_NAME);
    }
    if (!h) return nullptr;
    void* p = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(M2T2Shared));
    if (!p) { CloseHandle(h); return nullptr; }
    if (outHandle) *outHandle = h;
    return reinterpret_cast<M2T2Shared*>(p);
}

#pragma once
#include <cstdint>

// Metin2 packet header enum'ları (bilinen protokolden)
enum PacketHeader : uint8_t {
    // Client -> Server
    HEADER_CG_ATTACK            = 0x0F,
    HEADER_CG_CHAT              = 0x05,
    HEADER_CG_CHARACTER_MOVE    = 0x02,
    HEADER_CG_USE_SKILL         = 0x52,
    HEADER_CG_ITEM_PICK_UP      = 0x1E,
    HEADER_CG_ITEM_USE          = 0x1C,
    HEADER_CG_ITEM_DROP         = 0x1D,

    // Server -> Client
    HEADER_GC_CHARACTER_ADD     = 0x01,
    HEADER_GC_CHARACTER_DEL     = 0x81,
    HEADER_GC_MOVE              = 0x03,
    HEADER_GC_ATTACK            = 0x0E,
    HEADER_GC_CHAR_ADDITIONAL   = 0x08,
    HEADER_GC_ITEM_SET          = 0x1C,
    HEADER_GC_ITEM_DEL          = 0x1D,
    HEADER_GC_ITEM_GROUND_ADD   = 0x1E,
    HEADER_GC_ITEM_GROUND_DEL   = 0x1F,
    HEADER_GC_HP_SP             = 0x33,
    HEADER_GC_STUN              = 0x0B,
    HEADER_GC_DEAD              = 0x0D,
    HEADER_GC_EXP_UPDATE        = 0x26,
};

#pragma pack(push, 1)

// Hareket paketi (C->S)
struct TPacketCGMove {
    uint8_t  header;   // HEADER_CG_CHARACTER_MOVE
    uint8_t  func;     // hareket tipi: 0=걷기, 1=달리기 gibi
    uint8_t  arg;
    uint32_t time;
    int32_t  x, y;
};

// Saldırı paketi (C->S)
struct TPacketCGAttack {
    uint8_t  header;   // HEADER_CG_ATTACK
    uint8_t  bType;    // saldırı tipi
    uint32_t dwVID;    // hedef VID
    uint32_t dwTime;
};

// Skill kullanma (C->S)
struct TPacketCGUseSkill {
    uint8_t  header;
    uint32_t dwVnum;   // skill no
    uint8_t  bGrade;
    uint32_t dwVID;    // hedef
};

// Item toplama (C->S)
struct TPacketCGPickUp {
    uint8_t  header;
    uint32_t dwVID;
};

// HP/SP güncelleme (S->C)
struct TPacketGCHpSp {
    uint8_t  header;
    int32_t  hp, hpMax;
    int32_t  sp, spMax;
};

// Karakter ekleme (S->C)  — basitleştirilmiş
struct TPacketGCCharacterAdd {
    uint8_t  header;
    uint32_t dwVID;
    uint8_t  bType;    // 0=PC, 1=NPC, 2=MOB, ...
    uint32_t dwRace;
    int32_t  x, y, z;
    float    rot;
    int32_t  hp, hpMax;
    char     szName[24];
};

// Zemine düşmüş item (S->C)
struct TPacketGCItemGroundAdd {
    uint8_t  header;
    uint32_t dwVID;
    uint32_t dwVnum;
    int32_t  x, y;
};

#pragma pack(pop)

// 3D konum vektörü
struct Vec3 {
    float x, y, z;
    float DistanceTo(const Vec3& o) const {
        float dx = x - o.x, dy = y - o.y;
        return sqrtf(dx*dx + dy*dy);
    }
};

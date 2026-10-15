#pragma once
#include <cstdint>

// !! Tüm offsetler sunucunun client binary'sine göre değişir.
// Cheat Engine veya IDA ile kendi clientının offsetlerini bul
// ve aşağıdaki değerleri güncelle.

namespace Offsets {

// --- Temel pointer'lar ---
// CPythonPlayer singleton (global işaretçi adresi)
constexpr uintptr_t PYTHON_PLAYER_PTR = 0x00000000;

// CPythonCharacterManager singleton
constexpr uintptr_t CHAR_MANAGER_PTR  = 0x00000000;

// CNetworkStream singleton (send/recv için)
constexpr uintptr_t NET_STREAM_PTR    = 0x00000000;

// --- CPythonPlayer member offsetleri ---
namespace Player {
    constexpr uintptr_t HP          = 0x000;  // int
    constexpr uintptr_t HP_MAX      = 0x004;  // int
    constexpr uintptr_t MP          = 0x008;  // int
    constexpr uintptr_t MP_MAX      = 0x00C;  // int
    constexpr uintptr_t LEVEL       = 0x010;  // int
    constexpr uintptr_t EXP         = 0x014;  // uint64
    constexpr uintptr_t POS_X       = 0x020;  // float
    constexpr uintptr_t POS_Y       = 0x024;  // float
    constexpr uintptr_t POS_Z       = 0x028;  // float
    constexpr uintptr_t VID         = 0x02C;  // uint32 — karakter virtual ID
}

// --- CInstanceBase member offsetleri (her entity için) ---
namespace Entity {
    constexpr uintptr_t VID         = 0x000;  // uint32
    constexpr uintptr_t RACE        = 0x004;  // uint32 (mob tipi)
    constexpr uintptr_t HP          = 0x008;  // int
    constexpr uintptr_t HP_MAX      = 0x00C;  // int
    constexpr uintptr_t POS_X       = 0x010;  // float
    constexpr uintptr_t POS_Y       = 0x014;  // float
    constexpr uintptr_t NAME        = 0x018;  // char[64]
    constexpr uintptr_t IS_DEAD     = 0x058;  // bool
    constexpr uintptr_t IS_MONSTER  = 0x05C;  // bool
}

// --- CNetworkStream — send/recv fonksiyon adresleri ---
// Pattern scan ile de bulunabilir (PatternScan kullan)
namespace Net {
    constexpr uintptr_t SEND_FN     = 0x00000000;  // CNetworkStream::Send
    constexpr uintptr_t RECV_FN     = 0x00000000;  // CNetworkStream::Recv
    constexpr uintptr_t SEND_MOVE   = 0x00000000;  // SendMove paketi gönderen fn
    constexpr uintptr_t SEND_ATTACK = 0x00000000;  // SendAttack
    constexpr uintptr_t SEND_SKILL  = 0x00000000;  // SendUseSkill
    constexpr uintptr_t SEND_PICKUP = 0x00000000;  // SendPickup (item topla)
}

} // namespace Offsets

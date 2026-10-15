#pragma once
#include <cstdint>

// Adresler 14.10.2016 tarihli Metin2 client dump'ından alınmıştır.
// Bu fonksiyonlar Python C extension (PyCFunction) imzasıyla çağrılır:
//   PyObject* fn(PyObject* self, PyObject* args)

namespace Offsets {

// --- module: playerm2g2 ---
namespace Player {
    constexpr uintptr_t GetMainCharacterPosition = 0x3454370; // () -> (x,y,z)
    constexpr uintptr_t GetMainCharacterIndex    = 0x3454310; // () -> int
    constexpr uintptr_t GetMainCharacterName     = 0x3454340; // () -> str
    constexpr uintptr_t GetStatus                = 0x3456FF0; // (status_id) -> int  (HP=0,MaxHP=1,SP=2,MaxSP=3)
    constexpr uintptr_t GetEXP                   = 0x34509E0; // () -> int
    constexpr uintptr_t GetLevel                 = 0x34569E0; // () -> int  (GetSkillLevel, playerm2g2)
    constexpr uintptr_t GetMoney                 = 0x3450A20; // () -> int
    constexpr uintptr_t SetTarget                = 0x345B6C0; // (vid) -> None
    constexpr uintptr_t GetTargetVID             = 0x3457120; // () -> int
    constexpr uintptr_t PlayerMoveStop           = 0x3459570; // () -> None
    constexpr uintptr_t GetRace                  = 0x34565A0; // () -> int
}

// --- module: m2netm2g ---
namespace Net {
    constexpr uintptr_t SendOnClickPacket        = 0x33C4910; // (vid) -> None  (saldırı/interact)
    constexpr uintptr_t SendItemUsePacket        = 0x33C7260; // (slot) -> None
    constexpr uintptr_t SendItemDropPacketNew    = 0x33C6F50; // (slot, count) -> None
    constexpr uintptr_t SendChatPacket           = 0x33C56A0; // (type, msg) -> None
    constexpr uintptr_t SendCommandPacket        = 0x33C5790; // (cmd) -> None
}

// --- module: chrmgrm2g ---
namespace ChrMgr {
    constexpr uintptr_t GetPickedVID             = 0x32D97E0; // () -> int
    constexpr uintptr_t IsDead                   = 0x32D99C0; // (vid) -> bool
    constexpr uintptr_t IsPC                     = 0x32D9B10; // (vid) -> bool
    constexpr uintptr_t GetVIDInfo               = 0x32D9830; // (vid) -> dict
}

// --- module: nonplayer ---
namespace NonPlayer {
    constexpr uintptr_t GetLevelByVID            = 0x3405920; // (vid) -> int
    constexpr uintptr_t GetMonsterName           = 0x34059F0; // (vid) -> str
    constexpr uintptr_t GetVnumByVID             = 0x3405A50; // (vid) -> int
}

// --- module: chr ---
namespace Chr {
    constexpr uintptr_t Pick                     = 0x32DCE60; // () -> int (vid)
    constexpr uintptr_t GetNameByVID             = 0x32DC850; // (vid) -> str
    constexpr uintptr_t GetProjectPosition       = 0x32DC8D0; // (vid) -> (x,y,z)
    constexpr uintptr_t IsEnemy                  = 0x32DCB40; // (vid) -> bool
    constexpr uintptr_t IsNPC                    = 0x32DCC50; // (vid) -> bool
    constexpr uintptr_t IsShop                   = 0x32DCD50; // (vid) -> bool
}

} // namespace Offsets

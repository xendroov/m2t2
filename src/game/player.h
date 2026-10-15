#pragma once
#include "structs.h"
#include <cstdint>

// Lokal oyuncu (CPythonPlayer singleton wrapper)
namespace Player {
    uint32_t GetVID();
    int32_t  GetHP();
    int32_t  GetHPMax();
    int32_t  GetMP();
    int32_t  GetMPMax();
    int32_t  GetLevel();
    uint64_t GetExp();
    Vec3     GetPos();
    float    GetHPPercent();
    float    GetMPPercent();

    // --- Aksiyon fonksiyonları ---
    // Bu fonksiyonlar oyunun kendi send* metodlarını çağırır.
    // Offset'ler offsets.h -> Net:: altındadır.
    void     Attack(uint32_t targetVID);
    void     UseSkill(uint32_t skillVnum, uint32_t targetVID);
    void     MoveTo(float x, float y);
    void     PickUp(uint32_t itemVID);
}

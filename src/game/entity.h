#pragma once
#include "structs.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

// CInstanceBase'i sarmalayan sınıf — pointer'dan okur
struct Entity {
    uintptr_t base = 0;

    bool Valid() const { return base != 0; }

    uint32_t    GetVID()      const;
    uint32_t    GetRace()     const;
    int32_t     GetHP()       const;
    int32_t     GetHPMax()    const;
    Vec3        GetPos()      const;
    std::string GetName()     const;
    bool        IsDead()      const;
    bool        IsMonster()   const;

    float DistanceTo(const Entity& other) const {
        return GetPos().DistanceTo(other.GetPos());
    }
    float DistanceTo(const Vec3& pos) const {
        return GetPos().DistanceTo(pos);
    }
};

// CPythonCharacterManager wrapper — tüm entity'leri döner
namespace EntityManager {
    std::vector<Entity> GetAll();
    Entity              GetByVID(uint32_t vid);

    // Yakındaki tüm canlı monster'ları mesafeye göre sıralı döner
    std::vector<Entity> GetNearbyMonsters(const Vec3& from, float maxDist);
}

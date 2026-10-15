#pragma once
#include "structs.h"
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

// Oyundaki bir entity'yi VID üzerinden temsil eder.
// Tüm veri sorguları PyAPI üzerinden canlı olarak çekilir.
struct Entity {
    uint32_t vid = 0;

    bool Valid() const { return vid != 0; }

    uint32_t    GetVID()      const;
    uint32_t    GetRace()     const;
    int32_t     GetHP()       const;
    int32_t     GetHPMax()    const;
    Vec3        GetPos()      const;
    std::string GetName()     const;
    bool        IsDead()      const;
    bool        IsMonster()   const;

    float DistanceTo(const Entity& other) const { return GetPos().DistanceTo(other.GetPos()); }
    float DistanceTo(const Vec3& pos)     const { return GetPos().DistanceTo(pos); }
};

// Packet hook'larından beslenen entity kayıt sistemi
namespace EntityManager {
    void RegisterEntity(uint32_t vid);  // HEADER_GC_CHARACTER_ADD gelince çağrılır
    void RemoveEntity(uint32_t vid);    // HEADER_GC_CHARACTER_DEL gelince çağrılır

    std::vector<Entity> GetAll();
    Entity              GetByVID(uint32_t vid);
    std::vector<Entity> GetNearbyMonsters(const Vec3& from, float maxDist);
}

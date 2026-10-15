#include "entity.h"
#include "offsets.h"
#include "../core/memory.h"
#include <algorithm>
#include <cstring>

using namespace Memory;
using namespace Offsets;

uint32_t Entity::GetVID()  const { return Read<uint32_t>(base + Entity::VID); }
uint32_t Entity::GetRace() const { return Read<uint32_t>(base + Entity::RACE); }
int32_t  Entity::GetHP()   const { return Read<int32_t> (base + Entity::HP); }
int32_t  Entity::GetHPMax()const { return Read<int32_t> (base + Entity::HP_MAX); }
bool     Entity::IsDead()  const { return Read<bool>    (base + Entity::IS_DEAD); }
bool     Entity::IsMonster()const{ return Read<bool>    (base + Entity::IS_MONSTER); }

Vec3 Entity::GetPos() const {
    return {
        Read<float>(base + Entity::POS_X),
        Read<float>(base + Entity::POS_Y),
        0.f
    };
}

std::string Entity::GetName() const {
    char buf[64]{};
    // Entity::NAME offsetindeki char dizisini kopyala
    __try {
        memcpy(buf, reinterpret_cast<void*>(base + Entity::NAME), 63);
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return std::string(buf);
}

// -----------------------------------------------------------------------
// CPythonCharacterManager — entity listesine erişim
// Metin2 source'unda bu genellikle std::map<DWORD, CInstanceBase*>
// Offset'ler clienta göre değişir; aşağısı şablon.
// -----------------------------------------------------------------------
namespace EntityManager {

// Karakter haritasının başlangıç pointer'ı (CPythonCharacterManager::m_CharInstanceMap)
static constexpr uintptr_t CHAR_MAP_BEGIN = 0x00000000; // Cheat Engine ile bul
static constexpr uintptr_t CHAR_MAP_END   = 0x00000004;

std::vector<Entity> GetAll() {
    std::vector<Entity> result;

    // std::map<uint32_t, CInstanceBase*> yapısını iterate etmek için
    // önce map boyutunu oku, sonra her node'u gez.
    // Bu kısım clienta göre tamamen değişir — PatternScan + reversing gerekir.
    // Şimdilik taslak olarak bırakılıyor.

    return result;
}

Entity GetByVID(uint32_t vid) {
    for (auto& e : GetAll())
        if (e.GetVID() == vid) return e;
    return {};
}

std::vector<Entity> GetNearbyMonsters(const Vec3& from, float maxDist) {
    auto all = GetAll();
    std::vector<Entity> result;

    for (auto& e : all) {
        if (!e.Valid() || e.IsDead() || !e.IsMonster()) continue;
        if (e.DistanceTo(from) <= maxDist)
            result.push_back(e);
    }

    std::sort(result.begin(), result.end(), [&](const Entity& a, const Entity& b) {
        return a.DistanceTo(from) < b.DistanceTo(from);
    });

    return result;
}

} // namespace EntityManager

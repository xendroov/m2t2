#include "entity.h"
#include "pyapi.h"
#include "offsets.h"
#include <algorithm>
#include <tuple>

using namespace PyAPI;
using namespace Offsets;

// Entity alanları: Chr modülü GetProjectPosition(vid)->(x,y,z), GetNameByVID(vid)->str
// IsDead/IsPC de chrmgr modülünden geliyor

uint32_t Entity::GetVID()   const { return vid; }
bool     Entity::IsDead()   const { return CallBool_Int(ChrMgr::IsDead, (int)vid); }
bool     Entity::IsMonster()const {
    // Oyuncu değilse ve NPC değilse monster
    if (CallBool_Int(ChrMgr::IsPC, (int)vid)) return false;
    if (CallBool_Int(Chr::IsNPC,   (int)vid)) return false;
    if (CallBool_Int(Chr::IsShop,  (int)vid)) return false;
    return true;
}

Vec3 Entity::GetPos() const {
    auto xyz = CallXYZ_Int(Chr::GetProjectPosition, (int)vid);
    return { std::get<0>(xyz), std::get<1>(xyz), std::get<2>(xyz) };
}

std::string Entity::GetName() const {
    return CallStr_Int(Chr::GetNameByVID, (int)vid);
}

int32_t Entity::GetHP()    const { return 0; } // Mob HP Python API'sinde doğrudan yok
int32_t Entity::GetHPMax() const { return 1; }
uint32_t Entity::GetRace() const { return (uint32_t)CallInt_Int(NonPlayer::GetVnumByVID, (int)vid); }

// -----------------------------------------------------------------------
// EntityManager — dikkat: Python API VID listesini doğrudan sunmaz.
// Yakın entity bulmak için chr.Pick() ve GetPickedVID() kullanılır,
// ya da zemin tıklama / kamera tarama yapılır.
// Gerçek bir entity listesi için packet sniffer'dan
// HEADER_GC_CHARACTER_ADD / DEL takibi şarttır (send_hook.cpp).
// -----------------------------------------------------------------------
namespace EntityManager {

// Bot, gördüğü entity'leri packet hook üzerinden bu listeye ekler.
static std::vector<Entity> g_knownEntities;
static std::mutex           g_mutex;

void RegisterEntity(uint32_t vid) {
    std::lock_guard<std::mutex> lk(g_mutex);
    for (auto& e : g_knownEntities) if (e.vid == vid) return;
    g_knownEntities.push_back({vid});
}

void RemoveEntity(uint32_t vid) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_knownEntities.erase(
        std::remove_if(g_knownEntities.begin(), g_knownEntities.end(),
            [vid](const Entity& e){ return e.vid == vid; }),
        g_knownEntities.end()
    );
}

std::vector<Entity> GetAll() {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_knownEntities;
}

Entity GetByVID(uint32_t vid) {
    std::lock_guard<std::mutex> lk(g_mutex);
    for (auto& e : g_knownEntities) if (e.vid == vid) return e;
    return {};
}

std::vector<Entity> GetNearbyMonsters(const Vec3& from, float maxDist) {
    auto all = GetAll();
    std::vector<Entity> result;
    for (auto& e : all) {
        if (!e.Valid() || e.IsDead() || !e.IsMonster()) continue;
        if (e.DistanceTo(from) <= maxDist) result.push_back(e);
    }
    std::sort(result.begin(), result.end(), [&](const Entity& a, const Entity& b){
        return a.DistanceTo(from) < b.DistanceTo(from);
    });
    return result;
}

} // namespace EntityManager

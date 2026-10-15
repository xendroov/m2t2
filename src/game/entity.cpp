#include "entity.h"
#include "pyapi.h"
#include <algorithm>
#include <tuple>
#include <cstdio>

uint32_t Entity::GetVID()   const { return vid; }

bool Entity::IsDead() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "chrmgr.IsDead(%u)", vid);
    return PyAPI::EvalBool(buf);
}

bool Entity::IsMonster() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "chrmgr.IsPC(%u)", vid);
    if (PyAPI::EvalBool(buf)) return false;
    snprintf(buf, sizeof(buf), "chr.IsNPC(%u)", vid);
    if (PyAPI::EvalBool(buf)) return false;
    snprintf(buf, sizeof(buf), "chr.IsShop(%u)", vid);
    if (PyAPI::EvalBool(buf)) return false;
    return true;
}

Vec3 Entity::GetPos() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "chr.GetProjectPosition(%u)", vid);
    auto xyz = PyAPI::EvalXYZ(buf);
    return { std::get<0>(xyz), std::get<1>(xyz), std::get<2>(xyz) };
}

std::string Entity::GetName() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "chr.GetNameByVID(%u)", vid);
    return PyAPI::EvalStr(buf);
}

int32_t  Entity::GetHP()    const { return 0; }
int32_t  Entity::GetHPMax() const { return 1; }

uint32_t Entity::GetRace() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "nonplayer.GetVnumByVID(%u)", vid);
    return (uint32_t)PyAPI::EvalInt(buf);
}

// -----------------------------------------------------------------------
// EntityManager
// -----------------------------------------------------------------------
namespace EntityManager {

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

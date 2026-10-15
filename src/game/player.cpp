#include "player.h"
#include "offsets.h"
#include "structs.h"
#include "../core/memory.h"

using namespace Memory;
using namespace Offsets;

// CPythonPlayer singleton'ının base adresini çöz
static uintptr_t GetPlayerBase() {
    return ResolvePointer(PYTHON_PLAYER_PTR, {0});
}

namespace Player {

uint32_t GetVID()   { auto b = GetPlayerBase(); return b ? Read<uint32_t>(b + Offsets::Player::VID)   : 0; }
int32_t  GetHP()    { auto b = GetPlayerBase(); return b ? Read<int32_t> (b + Offsets::Player::HP)    : 0; }
int32_t  GetHPMax() { auto b = GetPlayerBase(); return b ? Read<int32_t> (b + Offsets::Player::HP_MAX): 1; }
int32_t  GetMP()    { auto b = GetPlayerBase(); return b ? Read<int32_t> (b + Offsets::Player::MP)    : 0; }
int32_t  GetMPMax() { auto b = GetPlayerBase(); return b ? Read<int32_t> (b + Offsets::Player::MP_MAX): 1; }
int32_t  GetLevel() { auto b = GetPlayerBase(); return b ? Read<int32_t> (b + Offsets::Player::LEVEL) : 0; }
uint64_t GetExp()   { auto b = GetPlayerBase(); return b ? Read<uint64_t>(b + Offsets::Player::EXP)   : 0; }

Vec3 GetPos() {
    auto b = GetPlayerBase();
    if (!b) return {};
    return {
        Read<float>(b + Offsets::Player::POS_X),
        Read<float>(b + Offsets::Player::POS_Y),
        0.f
    };
}

float GetHPPercent() {
    auto mx = GetHPMax();
    return mx ? (float)GetHP() / mx * 100.f : 0.f;
}

float GetMPPercent() {
    auto mx = GetMPMax();
    return mx ? (float)GetMP() / mx * 100.f : 0.f;
}

// -------------------------------------------------------------------
// Aksiyon çağrıları — oyunun kendi fonksiyonlarını thiscall ile çağır
// -------------------------------------------------------------------

// CNetworkStream::SendMove(func, arg, time, x, y)
typedef void(__thiscall* tSendMove)(void* thisptr, uint8_t func, uint8_t arg, uint32_t time, int32_t x, int32_t y);
typedef void(__thiscall* tSendAttack)(void* thisptr, uint8_t type, uint32_t vid, uint32_t time);
typedef void(__thiscall* tSendSkill)(void* thisptr, uint32_t vnum, uint8_t grade, uint32_t vid);
typedef void(__thiscall* tSendPickup)(void* thisptr, uint32_t vid);

static void* GetNetStream() {
    return reinterpret_cast<void*>(ResolvePointer(NET_STREAM_PTR, {0}));
}

void Attack(uint32_t targetVID) {
    void* net = GetNetStream();
    if (!net || !Net::SEND_ATTACK) return;
    auto fn = reinterpret_cast<tSendAttack>(Net::SEND_ATTACK);
    fn(net, 0, targetVID, GetTickCount());
}

void UseSkill(uint32_t skillVnum, uint32_t targetVID) {
    void* net = GetNetStream();
    if (!net || !Net::SEND_SKILL) return;
    auto fn = reinterpret_cast<tSendSkill>(Net::SEND_SKILL);
    fn(net, skillVnum, 0, targetVID);
}

void MoveTo(float x, float y) {
    void* net = GetNetStream();
    if (!net || !Net::SEND_MOVE) return;
    auto fn = reinterpret_cast<tSendMove>(Net::SEND_MOVE);
    fn(net, 0, 0, GetTickCount(), (int32_t)x, (int32_t)y);
}

void PickUp(uint32_t itemVID) {
    void* net = GetNetStream();
    if (!net || !Net::SEND_PICKUP) return;
    auto fn = reinterpret_cast<tSendPickup>(Net::SEND_PICKUP);
    fn(net, itemVID);
}

} // namespace Player

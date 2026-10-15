#include "player.h"
#include "pyapi.h"

#include <cstdio>

namespace Player {

Vec3 GetPos() {
    auto xyz = PyAPI::EvalXYZ("player.GetMainCharacterPosition()");
    return { std::get<0>(xyz), std::get<1>(xyz), std::get<2>(xyz) };
}

// player.HP, player.MAX_HP vb. Python modülündeki sabitler — ham integer (0,1,2,3) DEĞİL
int32_t GetHP()    { return PyAPI::EvalInt("player.GetStatus(player.HP)"); }
int32_t GetHPMax() { return PyAPI::EvalInt("player.GetStatus(player.MAX_HP)"); }
int32_t GetMP()    { return PyAPI::EvalInt("player.GetStatus(player.SP)"); }
int32_t GetMPMax() { return PyAPI::EvalInt("player.GetStatus(player.MAX_SP)"); }

int32_t  GetLevel() { return PyAPI::EvalInt("player.GetStatus(player.LEVEL)"); }
uint32_t GetVID()   { return (uint32_t)PyAPI::EvalInt("player.GetMainCharacterIndex()"); }
uint64_t GetExp()   { return (uint64_t)PyAPI::EvalInt("player.GetEXP()"); }

float GetHPPercent() {
    auto mx = GetHPMax();
    return mx > 0 ? (float)GetHP() / mx * 100.f : 0.f;
}

float GetMPPercent() {
    auto mx = GetMPMax();
    return mx > 0 ? (float)GetMP() / mx * 100.f : 0.f;
}

void Attack(uint32_t targetVID) {
    char buf[64];
    snprintf(buf, sizeof(buf), "net.SendOnClickPacket(%u)", targetVID);
    PyAPI::Exec(buf);
}

void UseSkill(uint32_t /*skillVnum*/, uint32_t targetVID) {
    char buf[64];
    snprintf(buf, sizeof(buf), "player.SetTarget(%u)", targetVID);
    PyAPI::Exec(buf);
    snprintf(buf, sizeof(buf), "net.SendOnClickPacket(%u)", targetVID);
    PyAPI::Exec(buf);
}

void MoveTo(float /*x*/, float /*y*/) {
    // Python API'de koordinat bazlı move paketi yok
}

void PickUp(uint32_t itemVID) {
    char buf[64];
    snprintf(buf, sizeof(buf), "net.SendOnClickPacket(%u)", itemVID);
    PyAPI::Exec(buf);
}

} // namespace Player

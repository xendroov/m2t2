#include "player.h"
#include "pyapi.h"
#include "offsets.h"

// Offsets:: prefix ile kullan — "namespace Player" ile çakışmasın
namespace O = Offsets;

namespace Player {

Vec3 GetPos() {
    auto xyz = PyAPI::CallXYZ_Void(O::Player::GetMainCharacterPosition);
    return { std::get<0>(xyz), std::get<1>(xyz), std::get<2>(xyz) };
}

int32_t GetHP()    { return PyAPI::CallInt_Int(O::Player::GetStatus, 0); }
int32_t GetHPMax() { return PyAPI::CallInt_Int(O::Player::GetStatus, 1); }
int32_t GetMP()    { return PyAPI::CallInt_Int(O::Player::GetStatus, 2); }
int32_t GetMPMax() { return PyAPI::CallInt_Int(O::Player::GetStatus, 3); }

int32_t  GetLevel() { return PyAPI::CallInt_Void(O::Player::GetMainCharacterIndex); }
uint32_t GetVID()   { return (uint32_t)PyAPI::CallInt_Void(O::Player::GetMainCharacterIndex); }
uint64_t GetExp()   { return (uint64_t)PyAPI::CallInt_Void(O::Player::GetEXP); }

float GetHPPercent() {
    auto mx = GetHPMax();
    return mx > 0 ? (float)GetHP() / mx * 100.f : 0.f;
}

float GetMPPercent() {
    auto mx = GetMPMax();
    return mx > 0 ? (float)GetMP() / mx * 100.f : 0.f;
}

void Attack(uint32_t targetVID) {
    PyAPI::CallVoid_Int(O::Net::SendOnClickPacket, (int)targetVID);
}

void UseSkill(uint32_t /*skillVnum*/, uint32_t targetVID) {
    PyAPI::CallVoid_Int(O::Player::SetTarget, (int)targetVID);
    PyAPI::CallVoid_Int(O::Net::SendOnClickPacket, (int)targetVID);
}

void MoveTo(float /*x*/, float /*y*/) {
    // Python API'de koordinat bazlı move paketi yok; ileriki sürümde eklenecek
}

void PickUp(uint32_t itemVID) {
    PyAPI::CallVoid_Int(O::Net::SendOnClickPacket, (int)itemVID);
}

} // namespace Player

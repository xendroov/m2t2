#include "player.h"
#include "pyapi.h"
#include "offsets.h"

using namespace PyAPI;
using namespace Offsets;

// GetStatus(status_id) — Metin2 kaynak: STATUS_HP=0, STATUS_MAX_HP=1, STATUS_SP=2, STATUS_MAX_SP=3
namespace Player {

Vec3 GetPos() {
    auto [x, y, z] = CallXYZ_Void(Player::GetMainCharacterPosition);
    return {x, y, z};
}

int32_t GetHP()    { return CallInt_Int(Player::GetStatus, 0); }
int32_t GetHPMax() { return CallInt_Int(Player::GetStatus, 1); }
int32_t GetMP()    { return CallInt_Int(Player::GetStatus, 2); }
int32_t GetMPMax() { return CallInt_Int(Player::GetStatus, 3); }

int32_t GetLevel() { return CallInt_Void(Player::GetMainCharacterIndex); }
uint32_t GetVID()  { return (uint32_t)CallInt_Void(Player::GetMainCharacterIndex); }

uint64_t GetExp()  { return (uint64_t)CallInt_Void(Player::GetEXP); }

float GetHPPercent() {
    auto mx = GetHPMax();
    return mx > 0 ? (float)GetHP() / mx * 100.f : 0.f;
}

float GetMPPercent() {
    auto mx = GetMPMax();
    return mx > 0 ? (float)GetMP() / mx * 100.f : 0.f;
}

// --- Aksiyon gönderenler ---

void Attack(uint32_t targetVID) {
    // SendOnClickPacket(vid) — sunucu saldırıyı tetikler
    CallVoid_Int(Net::SendOnClickPacket, (int)targetVID);
}

void UseSkill(uint32_t /*skillVnum*/, uint32_t targetVID) {
    // Skill kullanımı: önce SetTarget, sonra SendOnClickPacket
    // Gerçek skill slot tetiklemesi için UI katmanı gerekir;
    // şimdilik hedefi set edip click gönder.
    CallVoid_Int(Player::SetTarget, (int)targetVID);
    CallVoid_Int(Net::SendOnClickPacket, (int)targetVID);
}

void MoveTo(float /*x*/, float /*y*/) {
    // Metin2 Python API'sinde doğrudan coordinate bazlı move paketi yoktur.
    // SetTarget + SendOnClickPacket ile server-side yaklaşım kullan.
    // İleride mouse_event simülasyonu veya internal move packet hook eklenebilir.
}

void PickUp(uint32_t itemVID) {
    CallVoid_Int(Net::SendOnClickPacket, (int)itemVID);
}

} // namespace Player

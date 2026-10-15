#include "farm_state.h"
#include "../../game/player.h"
#include "../../game/entity.h"
#include "../../core/console.h"
#include <Windows.h>

static constexpr float ATTACK_RANGE  = 300.f;  // Saldırı menzili (koordinat birimi)
static constexpr uint32_t ATTACK_CD  = 800;    // ms — normal vuruş bekleme süresi
static constexpr uint32_t MOVE_CD    = 500;    // ms — hareket komutu bekleme süresi

void FarmState::OnEnter() {
    Console::Log("[Farm] Basladi — aralik=%.0f, skill=%u", m_range, m_skillVnum);
    m_targetVID = 0;
}

void FarmState::OnExit() {
    Console::Log("[Farm] Durdu");
    m_targetVID = 0;
}

void FarmState::SelectTarget() {
    Vec3 myPos = Player::GetPos();
    auto mobs  = EntityManager::GetNearbyMonsters(myPos, m_range);

    if (mobs.empty()) {
        m_targetVID = 0;
        return;
    }

    m_targetVID = mobs[0].GetVID();
    Console::Log("[Farm] Hedef: VID=%u  HP=%d  Mesafe=%.0f",
        m_targetVID, mobs[0].GetHP(), mobs[0].DistanceTo(myPos));
}

void FarmState::AttackTarget() {
    uint32_t now = GetTickCount();

    if (m_skillVnum > 0 && Player::GetMPPercent() > 20.f) {
        if (now - m_attackTick >= ATTACK_CD) {
            Player::UseSkill(m_skillVnum, m_targetVID);
            m_attackTick = now;
        }
    } else {
        if (now - m_attackTick >= ATTACK_CD) {
            Player::Attack(m_targetVID);
            m_attackTick = now;
        }
    }
}

void FarmState::Update() {
    // HP çok düşükse farm duraksatılabilir (üst katman Bot sınıfı yönetir)
    if (Player::GetHPPercent() < 15.f) return;

    // Hedef geçerliliğini kontrol et
    if (m_targetVID != 0) {
        Entity target = EntityManager::GetByVID(m_targetVID);
        if (!target.Valid() || target.IsDead()) {
            m_targetVID = 0;
        }
    }

    // Hedef yoksa yeni seç
    if (m_targetVID == 0) {
        SelectTarget();
        return;
    }

    Entity target = EntityManager::GetByVID(m_targetVID);
    if (!target.Valid()) { m_targetVID = 0; return; }

    Vec3 myPos = Player::GetPos();
    float dist = target.DistanceTo(myPos);

    if (dist > ATTACK_RANGE) {
        // Hedefin yanına git
        uint32_t now = GetTickCount();
        if (now - m_moveTick >= MOVE_CD) {
            Vec3 tPos = target.GetPos();
            Player::MoveTo(tPos.x, tPos.y);
            m_moveTick = now;
        }
    } else {
        AttackTarget();
    }
}

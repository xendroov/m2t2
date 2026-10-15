#pragma once
#include "state.h"
#include "../../game/structs.h"

// -----------------------------------------------------------------------
// FarmState — en yakın monster'ı hedef al, yaklaş, öldür, tekrarla.
// -----------------------------------------------------------------------
class FarmState : public IState {
public:
    explicit FarmState(float range = 1500.f, uint32_t skillVnum = 0)
        : m_range(range), m_skillVnum(skillVnum) {}

    void OnEnter() override;
    void OnExit()  override;
    void Update()  override;
    const char* Name() const override { return "Farm"; }

private:
    float    m_range;       // Arama yarıçapı (Metin2 koordinat birimi)
    uint32_t m_skillVnum;   // 0 = normal saldırı, >0 = skill kullan
    uint32_t m_targetVID = 0;
    uint32_t m_attackTick = 0;
    uint32_t m_moveTick   = 0;

    void SelectTarget();
    void AttackTarget();
};

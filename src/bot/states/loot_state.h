#pragma once
#include "state.h"
#include "../../game/structs.h"
#include <vector>
#include <cstdint>

// -----------------------------------------------------------------------
// LootState — yakındaki zemin itemleri toplar.
// PacketLogger üzerinden HEADER_GC_ITEM_GROUND_ADD paketlerini dinler
// ve görülen item VID'lerini kuyruğa alır.
// -----------------------------------------------------------------------
class LootState : public IState {
public:
    explicit LootState(float range = 600.f) : m_range(range) {}

    void OnEnter() override;
    void OnExit()  override;
    void Update()  override;
    const char* Name() const override { return "Loot"; }

    // Packet logger callback'inden çağrılır
    void OnItemGroundAdd(uint32_t vid, int32_t x, int32_t y);
    void OnItemGroundDel(uint32_t vid);

private:
    float    m_range;
    uint32_t m_pickupTick = 0;

    struct GroundItem { uint32_t vid; float x, y; };
    std::vector<GroundItem> m_items;
};

#include "loot_state.h"
#include "../../game/player.h"
#include "../../game/structs.h"
#include "../../network/packet_logger.h"
#include "../../core/console.h"
#include <algorithm>
#include <Windows.h>

static constexpr uint32_t PICKUP_CD = 300; // ms

void LootState::OnEnter() {
    Console::Log("[Loot] Basladi — aralik=%.0f", m_range);

    // Recv paketlerini dinle: zemin itemleri için
    PacketLogger::OnRecv([this](const uint8_t* data, size_t len) {
        if (len < 1) return;
        if (data[0] == HEADER_GC_ITEM_GROUND_ADD && len >= sizeof(TPacketGCItemGroundAdd)) {
            auto* pkt = reinterpret_cast<const TPacketGCItemGroundAdd*>(data);
            OnItemGroundAdd(pkt->dwVID, pkt->x, pkt->y);
        }
        if (data[0] == HEADER_GC_ITEM_GROUND_DEL && len >= 5) {
            uint32_t vid = *reinterpret_cast<const uint32_t*>(data + 1);
            OnItemGroundDel(vid);
        }
    });
}

void LootState::OnExit() {
    m_items.clear();
    Console::Log("[Loot] Durdu");
}

void LootState::OnItemGroundAdd(uint32_t vid, int32_t x, int32_t y) {
    m_items.push_back({vid, (float)x, (float)y});
}

void LootState::OnItemGroundDel(uint32_t vid) {
    m_items.erase(
        std::remove_if(m_items.begin(), m_items.end(),
            [vid](const GroundItem& g){ return g.vid == vid; }),
        m_items.end()
    );
}

void LootState::Update() {
    if (m_items.empty()) return;

    uint32_t now = GetTickCount();
    if (now - m_pickupTick < PICKUP_CD) return;

    Vec3 myPos = Player::GetPos();

    // En yakın item'i bul
    GroundItem* closest = nullptr;
    float minDist = m_range;

    for (auto& item : m_items) {
        Vec3 iPos{item.x, item.y, 0.f};
        float d = myPos.DistanceTo(iPos);
        if (d < minDist) {
            minDist = d;
            closest = &item;
        }
    }

    if (!closest) return;

    Player::PickUp(closest->vid);
    m_pickupTick = now;
}

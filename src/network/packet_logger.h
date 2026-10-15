#pragma once
#include <cstdint>
#include <string>
#include <functional>

// Paket gözlemcisi — hook kurulduktan sonra tüm gidip gelen paketleri dinler
namespace PacketLogger {

    // Callback tipleri
    using PacketCallback = std::function<void(const uint8_t* data, size_t len)>;

    void InstallHooks();
    void RemoveHooks();

    // Dışarıdan ek listener eklenebilir (bot modülleri için)
    void OnSend(PacketCallback cb);
    void OnRecv(PacketCallback cb);

    // İstatistik
    uint64_t SentCount();
    uint64_t RecvCount();
}

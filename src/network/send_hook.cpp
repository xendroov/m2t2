#include "../network/packet_logger.h"
#include "../core/hooks.h"
#include "../core/console.h"
#include "../game/offsets.h"
#include "../game/structs.h"
#include <vector>
#include <mutex>
#include <atomic>

// -----------------------------------------------------------------------
// CNetworkStream::Send ve Recv'i hook'layarak tüm paket trafiğini yakalar.
// -----------------------------------------------------------------------

namespace PacketLogger {

static std::vector<PacketCallback> g_sendCbs;
static std::vector<PacketCallback> g_recvCbs;
static std::mutex g_cbMutex;

static std::atomic<uint64_t> g_sentCount{0};
static std::atomic<uint64_t> g_recvCount{0};

static void* g_origSend = nullptr;
static void* g_origRecv = nullptr;

// Orijinal fonksiyon pointer'ları (thiscall)
// Send(this, const void* data, int size) — tipik imza
typedef bool(__thiscall* tOrigSend)(void* thisptr, const void* data, int size);
typedef bool(__thiscall* tOrigRecv)(void* thisptr, void* data, int size);

// -----------------------------------------------------------------------
// Hook fonksiyonları — __fastcall trick (MSVC thiscall hook için)
// ecx = this, edx = dummy, arg1 = data, arg2 = size
// -----------------------------------------------------------------------

bool __fastcall Hooked_Send(void* thisptr, void* /*edx*/, const void* data, int size) {
    if (size > 0 && data) {
        ++g_sentCount;
        Console::LogPacket("SEND >>", reinterpret_cast<const uint8_t*>(data), size);

        std::lock_guard<std::mutex> lk(g_cbMutex);
        for (auto& cb : g_sendCbs)
            cb(reinterpret_cast<const uint8_t*>(data), size);
    }
    return reinterpret_cast<tOrigSend>(g_origSend)(thisptr, data, size);
}

bool __fastcall Hooked_Recv(void* thisptr, void* /*edx*/, void* data, int size) {
    bool ret = reinterpret_cast<tOrigRecv>(g_origRecv)(thisptr, data, size);
    if (ret && size > 0 && data) {
        ++g_recvCount;
        Console::LogPacket("<< RECV", reinterpret_cast<const uint8_t*>(data), size);

        std::lock_guard<std::mutex> lk(g_cbMutex);
        for (auto& cb : g_recvCbs)
            cb(reinterpret_cast<const uint8_t*>(data), size);
    }
    return ret;
}

void InstallHooks() {
    if (Offsets::Net::SEND_FN) {
        Hooks::Install(Offsets::Net::SEND_FN,
                       reinterpret_cast<uintptr_t>(Hooked_Send),
                       &g_origSend);
        Console::Log("Send hook kuruldu: 0x%08X", Offsets::Net::SEND_FN);
    }
    if (Offsets::Net::RECV_FN) {
        Hooks::Install(Offsets::Net::RECV_FN,
                       reinterpret_cast<uintptr_t>(Hooked_Recv),
                       &g_origRecv);
        Console::Log("Recv hook kuruldu: 0x%08X", Offsets::Net::RECV_FN);
    }
}

void RemoveHooks() {
    if (g_origSend) Hooks::Remove(Offsets::Net::SEND_FN, g_origSend);
    if (g_origRecv) Hooks::Remove(Offsets::Net::RECV_FN, g_origRecv);
    g_origSend = g_origRecv = nullptr;
}

void OnSend(PacketCallback cb) {
    std::lock_guard<std::mutex> lk(g_cbMutex);
    g_sendCbs.push_back(std::move(cb));
}

void OnRecv(PacketCallback cb) {
    std::lock_guard<std::mutex> lk(g_cbMutex);
    g_recvCbs.push_back(std::move(cb));
}

uint64_t SentCount() { return g_sentCount.load(); }
uint64_t RecvCount() { return g_recvCount.load(); }

} // namespace PacketLogger

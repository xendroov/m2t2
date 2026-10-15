#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>

namespace Memory {

// Process içindeyiz (DLL), direkt pointer dereference yeterli —
// ama erişim ihlali yakalamak için SafeRead/SafeWrite kullan.

template<typename T>
T Read(uintptr_t addr) {
    return *reinterpret_cast<T*>(addr);
}

template<typename T>
void Write(uintptr_t addr, const T& val) {
    DWORD old;
    VirtualProtect(reinterpret_cast<void*>(addr), sizeof(T), PAGE_EXECUTE_READWRITE, &old);
    *reinterpret_cast<T*>(addr) = val;
    VirtualProtect(reinterpret_cast<void*>(addr), sizeof(T), old, &old);
}

// Crash vermeden okuma — SEH tabanlı
template<typename T>
bool SafeRead(uintptr_t addr, T& out) {
    __try {
        out = *reinterpret_cast<T*>(addr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// İşaretçi zinciri izleme: base + {off1, off2, ...}
uintptr_t ResolvePointer(uintptr_t base, const std::vector<uintptr_t>& offsets);

// Bellekte bir byte dizisi ara (AOB / pattern scan)
// pattern: "48 8B ? ? ? ? 48 8B" — ? joker
uintptr_t PatternScan(const char* pattern, const char* module = nullptr);

} // namespace Memory

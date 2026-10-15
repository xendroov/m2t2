#include "hooks.h"
#include <cstring>

namespace Hooks {

// 5-byte JMP = E9 <rel32>
static const int TRAMPOLINE_SIZE = 64;

bool Install(uintptr_t target, uintptr_t hook_fn, void** original_gate) {
    // Trampolin için çalıştırılabilir bellek al
    uint8_t* tramp = reinterpret_cast<uint8_t*>(
        VirtualAlloc(nullptr, TRAMPOLINE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)
    );
    if (!tramp) return false;

    // Orijinal ilk 5 byte'ı trampolene kopyala
    DWORD old;
    VirtualProtect(reinterpret_cast<void*>(target), 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(tramp, reinterpret_cast<void*>(target), 5);

    // Trampoline sonuna JMP -> (target + 5) ekle
    tramp[5] = 0xE9;
    *reinterpret_cast<int32_t*>(&tramp[6]) =
        static_cast<int32_t>((target + 5) - (reinterpret_cast<uintptr_t>(tramp) + 5 + 5));

    // Hedef fonksiyonun başına JMP -> hook_fn yaz
    *reinterpret_cast<uint8_t*>(target) = 0xE9;
    *reinterpret_cast<int32_t*>(target + 1) =
        static_cast<int32_t>(hook_fn - (target + 5));

    VirtualProtect(reinterpret_cast<void*>(target), 5, old, &old);

    *original_gate = tramp;
    return true;
}

void Remove(uintptr_t target, void* original_gate) {
    if (!original_gate) return;
    DWORD old;
    VirtualProtect(reinterpret_cast<void*>(target), 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(reinterpret_cast<void*>(target), original_gate, 5);
    VirtualProtect(reinterpret_cast<void*>(target), 5, old, &old);
    VirtualFree(original_gate, 0, MEM_RELEASE);
}

bool HookVTable(void* instance, int index, void* new_fn, void** old_fn) {
    void*** vtable = reinterpret_cast<void***>(instance);
    void** vt = *vtable;

    DWORD old;
    VirtualProtect(&vt[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
    *old_fn = vt[index];
    vt[index] = new_fn;
    VirtualProtect(&vt[index], sizeof(void*), old, &old);
    return true;
}

} // namespace Hooks

#pragma once
#include <Windows.h>
#include <cstdint>

// Basit x86 5-byte JMP detour hook motoru.
// İleri ileride MinHook veya Detours ile değiştirilebilir.

namespace Hooks {

// Fonksiyonu hookla: orijinal adrese 5-byte JMP yaz.
// original_gate: orjinal fonksiyonun ilk N byte'ını yürüten "trampolin" adresini döner.
bool Install(uintptr_t target, uintptr_t hook_fn, void** original_gate);

// Hook'u kaldır (orijinal byte'ları geri yaz).
void Remove(uintptr_t target, void* original_gate);

// VTable hook: sanal tablodaki bir girişi yeni adresle değiştirir.
bool HookVTable(void* instance, int index, void* new_fn, void** old_fn);

} // namespace Hooks

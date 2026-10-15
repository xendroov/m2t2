#include "memory.h"
#include <Psapi.h>
#include <sstream>
#include <vector>

namespace Memory {

uintptr_t ResolvePointer(uintptr_t base, const std::vector<uintptr_t>& offsets) {
    uintptr_t addr = base;
    for (size_t i = 0; i < offsets.size(); ++i) {
        uintptr_t tmp = 0;
        if (!SafeRead<uintptr_t>(addr, tmp)) return 0;
        addr = tmp + offsets[i];
    }
    return addr;
}

// Pattern: "48 8B ? ? ? ? 48" — ? joker karakter
static std::vector<std::pair<uint8_t, bool>> ParsePattern(const char* pat) {
    std::vector<std::pair<uint8_t, bool>> bytes;
    while (*pat) {
        if (*pat == ' ') { ++pat; continue; }
        if (*pat == '?') {
            bytes.push_back({0, false}); // wildcard
            ++pat;
            if (*pat == '?') ++pat;
        } else {
            bytes.push_back({(uint8_t)strtol(pat, nullptr, 16), true});
            pat += 2;
        }
    }
    return bytes;
}

uintptr_t PatternScan(const char* pattern, const char* moduleName) {
    HMODULE mod = moduleName ? GetModuleHandleA(moduleName) : GetModuleHandleA(nullptr);
    if (!mod) return 0;

    MODULEINFO info{};
    GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info));

    auto bytes = ParsePattern(pattern);
    auto* start = reinterpret_cast<uint8_t*>(info.lpBaseOfDll);
    size_t size  = info.SizeOfImage;

    for (size_t i = 0; i < size - bytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < bytes.size(); ++j) {
            if (bytes[j].second && start[i + j] != bytes[j].first) {
                found = false;
                break;
            }
        }
        if (found) return reinterpret_cast<uintptr_t>(&start[i]);
    }
    return 0;
}

} // namespace Memory

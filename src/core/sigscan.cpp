#include "sigscan.h"
#include <cstring>

namespace SigScan {

// PE header'dan modul araligini oku (Psapi'ye bagimlilik yok).
void MainModuleRange(uintptr_t& base, uintptr_t& end) {
    base = 0; end = 0;
    HMODULE mod = GetModuleHandleA(nullptr);
    if (!mod) return;
    base = (uintptr_t)mod;
    __try {
        uint8_t* p = (uint8_t*)base;
        uint32_t e_lfanew    = *(uint32_t*)(p + 0x3C);
        uint32_t sizeOfImage = *(uint32_t*)(p + e_lfanew + 0x50);
        end = base + sizeOfImage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        end = base + 0x05000000; // makul ust sinir (yaklasik 80MB)
    }
}

static bool RegionReadable(const MEMORY_BASIC_INFORMATION& mbi) {
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    DWORD p = mbi.Protect & 0xFF;
    return p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
           p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE ||
           p == PAGE_EXECUTE_WRITECOPY;
}

static bool RegionExecutable(const MEMORY_BASIC_INFORMATION& mbi) {
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    DWORD p = mbi.Protect & 0xFF;
    return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ ||
           p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

// Bir bellek bloğunda birebir arama (SEH korumali — sayfa yaris durumunda patlamaz).
// Ilk bayti memchr ile tarar (SIMD hizli) -> baslangic taramasi cok daha cabuk.
static uintptr_t ScanBlock(const uint8_t* start, size_t size,
                           const uint8_t* pat, size_t len) {
    if (size < len) return 0;
    __try {
        const uint8_t* p   = start;
        const uint8_t* end = start + (size - len) + 1;  // son olasi baslangic+1
        while (p < end) {
            const uint8_t* q = (const uint8_t*)memchr(p, pat[0], (size_t)(end - p));
            if (!q) break;
            size_t j = 1;
            for (; j < len; ++j) if (q[j] != pat[j]) break;
            if (j == len) return (uintptr_t)q;
            p = q + 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 0;
}

std::vector<uintptr_t> FindAllBytes(const uint8_t* pat, size_t len,
                                    bool execOnly, size_t cap) {
    std::vector<uintptr_t> hits;
    uintptr_t base, end;
    MainModuleRange(base, end);
    if (!base) return hits;

    uintptr_t addr = base;
    MEMORY_BASIC_INFORMATION mbi{};
    while (addr < end && VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t rbase = (uintptr_t)mbi.BaseAddress;
        uintptr_t rend  = rbase + mbi.RegionSize;
        bool ok = execOnly ? RegionExecutable(mbi) : RegionReadable(mbi);
        if (ok) {
            uintptr_t scanStart = rbase < base ? base : rbase;
            uintptr_t scanEnd   = rend  > end  ? end  : rend;
            uintptr_t cur = scanStart;
            while (cur < scanEnd) {
                uintptr_t m = ScanBlock((const uint8_t*)cur, scanEnd - cur, pat, len);
                if (!m) break;
                hits.push_back(m);
                if (hits.size() >= cap) return hits;
                cur = m + 1;
            }
        }
        addr = rend;
        if (rend <= rbase) break; // guard
    }
    return hits;
}

uintptr_t FindBytes(const uint8_t* pat, size_t len, bool execOnly) {
    auto v = FindAllBytes(pat, len, execOnly, 1);
    return v.empty() ? 0 : v[0];
}

uintptr_t FindString(const char* s) {
    return FindBytes((const uint8_t*)s, strlen(s), false);
}

uintptr_t FindFunctionStart(uintptr_t within, size_t maxBack) {
    __try {
        for (uintptr_t p = within; p > within - maxBack; --p) {
            const uint8_t* q = (const uint8_t*)p;
            // prolog: push ebp ; mov ebp,esp
            if (q[0] == 0x55 && q[1] == 0x8B && q[2] == 0xEC) {
                uint8_t prev  = *(const uint8_t*)(p - 1);
                uint8_t prev3 = *(const uint8_t*)(p - 3);
                if (prev == 0xCC || prev == 0xC3 || prev3 == 0xC2 || prev == 0x90)
                    return p;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 0;
}

} // namespace SigScan

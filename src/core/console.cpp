#include "console.h"
#include <cstdarg>
#include <ctime>

namespace Console {

static FILE* g_con = nullptr;

void Open() {
    AllocConsole();
    freopen_s(&g_con, "CONOUT$", "w", stdout);
    SetConsoleTitleA("[M2T2 Bot]");
    Log("=== M2T2 Bot basladi ===");
}

void Close() {
    if (g_con) { fclose(g_con); g_con = nullptr; }
    FreeConsole();
}

void Log(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, buf);
}

void LogPacket(const char* dir, const uint8_t* data, size_t len) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("[%02d:%02d:%02d] %s  header=0x%02X  len=%zu  |",
        st.wHour, st.wMinute, st.wSecond, dir, data[0], len);

    // İlk 16 byte hex dump
    size_t show = len < 16 ? len : 16;
    for (size_t i = 0; i < show; ++i) printf(" %02X", data[i]);
    if (len > 16) printf(" ...");
    printf("\n");
}

} // namespace Console

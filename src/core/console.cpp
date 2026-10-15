#include "console.h"
#include <cstdarg>
#include <ctime>

namespace Console {

static FILE* g_con = nullptr;

void Open() {
    // Eğer zaten konsol varsa önce ayır
    FreeConsole();
    AllocConsole();
    freopen_s(&g_con, "CONOUT$", "w", stdout);
    SetConsoleTitleA("[M2T2 Bot] - INSERT=ac/kapat  DELETE=cikis");

    // Konsol penceresini ön plana getir ve kapat düğmesini devre dışı bırak
    HWND hw = GetConsoleWindow();
    if (hw) {
        ShowWindow(hw, SW_SHOW);
        SetForegroundWindow(hw);
        // X butonunu kapat — kazara kapanmasın
        HMENU hm = GetSystemMenu(hw, FALSE);
        if (hm) DeleteMenu(hm, SC_CLOSE, MF_BYCOMMAND);
    }

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

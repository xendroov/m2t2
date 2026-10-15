#pragma once
#include <Windows.h>
#include <cstdio>
#include <cstdarg>

// Konsol olmadan dosyaya yazar — crash öncesi tanı için
namespace FileLog {

inline void Write(const char* fmt, ...) {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\m2t2bot_log.txt", "a");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);
}

} // namespace FileLog

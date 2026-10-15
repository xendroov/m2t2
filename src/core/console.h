#pragma once
#include <Windows.h>
#include <cstdio>
#include <string>

namespace Console {
    void Open();
    void Close();
    void Log(const char* fmt, ...);
    void LogPacket(const char* dir, const uint8_t* data, size_t len);
}

#pragma once
#include <atomic>
#include <Windows.h>

class Bot {
public:
    static Bot& Instance();
    void Start();
    void Stop();
    void OnFrame();   // D3D EndScene hook'tan (ANA THREAD) her karede cagrilir
    bool IsRunning() const { return m_running.load(); }
private:
    Bot() = default;
    void DoSetup();   // ilk karede bir kez: modul enjeksiyon + tani
    std::atomic<bool> m_running{ false };
    bool  m_setup_done = false;
    DWORD m_lastTick   = 0;
};

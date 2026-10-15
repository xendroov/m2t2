#pragma once
#include <atomic>

class Bot {
public:
    static Bot& Instance();
    void Start();
    void Stop();
    void Tick();
    bool IsRunning() const { return m_running.load(); }
private:
    Bot() = default;
    std::atomic<bool> m_running{ false };
};

#pragma once
#include "states/state.h"
#include <memory>
#include <atomic>

// Ana bot kontrolcüsü — state machine + tick döngüsü
class Bot {
public:
    static Bot& Instance();

    void Start();   // Arka plan thread'ini başlat
    void Stop();    // Thread'i durdur

    void SetState(std::unique_ptr<IState> state);
    IState* GetCurrentState() const { return m_state.get(); }

    bool IsRunning() const { return m_running.load(); }

private:
    Bot() = default;
    void Loop();    // Thread fonksiyonu

    std::unique_ptr<IState> m_state;
    std::atomic<bool>       m_running{false};
    HANDLE                  m_thread = nullptr;

    static constexpr uint32_t TICK_MS = 100; // Bot güncelleme sıklığı
};

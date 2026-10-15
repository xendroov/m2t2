#pragma once
#include <Windows.h>
#include "states/state.h"
#include <memory>
#include <atomic>

class Bot {
public:
    static Bot& Instance();

    void Start();
    void Stop();
    void SetState(std::unique_ptr<IState> state);

    bool IsRunning() const { return m_running.load(); }

    void Loop(); // thread fn'den erişildiği için public

private:
    Bot() = default;

    std::unique_ptr<IState> m_state;
    std::atomic<bool>       m_running{ false };
    HANDLE                  m_thread = nullptr;

    static constexpr DWORD TICK_MS = 100;
};

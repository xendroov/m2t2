#pragma once
#include <atomic>
#include <Windows.h>

class Bot {
public:
    static Bot& Instance();
    void Start();
    void Stop();
    void OnFrame();   // WndProc hook'tan (ANA THREAD) cagrilir
    void RequestDump() { m_dumpReq.store(true); }  // END tusu — bellek dump'i iste
    bool IsRunning() const { return m_running.load(); }
private:
    Bot() = default;
    void DoSetup();   // ilk karede bir kez: modul enjeksiyon + tani
    void DumpInfo();  // oyun fonksiyon adreslerini + kodlarini dosyaya yazar
    std::atomic<bool> m_dumpReq{ false };
    std::atomic<bool> m_running{ false };
    bool  m_setup_done = false;
    DWORD m_lastTick   = 0;
    DWORD m_lastStatus = 0;
    DWORD m_lastHpPot  = 0;
    DWORD m_lastSpPot  = 0;

public:
    // Ayarlar (ileride konsol komutuyla degistirilebilir)
    bool m_autoPotion = true;   // HP/SP dusunce iksir bas
    int  m_hpPotPct   = 50;     // HP bu yuzdenin altina dusunce
    int  m_spPotPct   = 30;     // SP bu yuzdenin altina dusunce
    int  m_hpPotKey   = 0x70;   // VK_F1 — HP iksiri bu quickslot'ta
    int  m_spPotKey   = 0x71;   // VK_F2 — MP iksiri bu quickslot'ta
};

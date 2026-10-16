#pragma once
#include <atomic>
#include <Windows.h>
#include <cstdint>
#include <vector>

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
    void ScanTarget();                    // yakindaki en yakin mob'u hedefle (tek hedef mod)
    void AttackArea();                    // menzildeki TUM mob'lara saldiri paketi
    std::vector<uint32_t> EnumerateVIDs(); // alive instance map'i gezer
    void ResolveAttack();                 // SendAttackPacket'i canli bellekte bul (bir kez)
    std::atomic<bool> m_dumpReq{ false };
    uint32_t m_targetVid = 0;
    DWORD    m_lastScan  = 0;
    std::atomic<bool> m_running{ false };
    bool  m_setup_done   = false;
    bool  m_attack_ready = false;         // ResolveAttack basarili oldu mu
    bool  m_fastFilter   = false;         // tek-cagrili Python filtre aktif mi (hiz)
    DWORD m_lastTick       = 0;
    DWORD m_lastStatus     = 0;
    DWORD m_lastStatusRead = 0;          // HP/SP batch okuma zamani
    DWORD m_lastHpPot      = 0;
    DWORD m_lastSpPot      = 0;
    DWORD m_farmPass       = 0;          // tarama sayaci (debug)
    int   m_curHp          = 0;          // son okunan HP (farm HP-kritik kontrolu)
    int   m_curMaxHp       = 0;

public:
    // Ayarlar (ileride konsol komutuyla degistirilebilir)
    bool m_autoFarm     = true;   // yakindaki mob'lari otomatik hedefle+vur
    bool m_autoPotion   = true;   // HP/SP dusunce iksir bas
    bool m_verbose      = true;   // AYRINTILI DEBUG: her mob icin neden vuruldu/atlandi
    int  m_hpPotPct     = 50;     // HP bu yuzdenin altina dusunce
    int  m_spPotPct     = 30;     // SP bu yuzdenin altina dusunce
    int  m_hpPotKey     = 0x70;   // VK_F1 — HP iksiri bu quickslot'ta
    int  m_spPotKey     = 0x71;   // VK_F2 — MP iksiri bu quickslot'ta
    float m_farmRange   = 2000.f; // forum onerisi: 2000 menzil
    int  m_attackType   = 0;      // SendAttackPacket bType (0 = normal vurus)
    int  m_maxTargets   = 7;      // forum: ayni anda max 7 mob (rate-limit)
    int  m_attackDelayMs= 200;    // forum: 180-220ms saldiri araligi
    char m_targetName[64] = {0};  // bos=her mob; doluysa sadece bu ismi iceren mob
};

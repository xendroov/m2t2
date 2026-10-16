// =====================================================================
//  m2t2gui.exe — M2T2 botunun HARICI AYAR PENCERESI
//  DLL ile paylasimli hafiza (shared_state.h) uzerinden konusur.
//  Oyun kapali olsa bile acilabilir; DLL inject olunca otomatik baglanir.
//  Saf Win32 — ek kutuphane yok (sadece comctl32: trackbar/slider).
// =====================================================================
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "shared/shared_state.h"

#pragma comment(lib, "comctl32.lib")

// ---- Kontrol kimlikleri ----
enum {
    ID_BOT = 1001, ID_MODE, ID_POT, ID_DEBUG, ID_SPEED,
    ID_RANGE, ID_DELAY, ID_MAXT, ID_ATYPE, ID_HPPCT, ID_SPPCT,
    ID_NAME
};

static HINSTANCE  g_hInst   = nullptr;
static M2T2Shared* g_shm    = nullptr;
static HANDLE     g_shmH    = nullptr;
static char       g_ini[MAX_PATH] = {0};
static bool       g_loading = false;   // kontrolleri shm'den yuklerken yazma

// Kontrol HWND'leri
static HWND hBot, hMode, hPot, hDebug, hSpeed;
static HWND hRange, hDelay, hMaxT, hAType, hHp, hSp;
static HWND hRangeV, hDelayV, hMaxTV, hATypeV, hHpV, hSpV;
static HWND hName;
static HWND hConn, hFn, hStHp, hStSp, hStLvl, hStFarm;

// ---- Kucuk yardimcilar ----
static HWND MkStatic(HWND p, const char* t, int x, int y, int w, int h) {
    return CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
                         x, y, w, h, p, nullptr, g_hInst, nullptr);
}
static HWND MkCheck(HWND p, const char* t, int id, int x, int y, int w) {
    return CreateWindowA("BUTTON", t, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                         x, y, w, 22, p, (HMENU)(INT_PTR)id, g_hInst, nullptr);
}
static HWND MkTrack(HWND p, int id, int x, int y, int w, int mn, int mx) {
    HWND t = CreateWindowA(TRACKBAR_CLASSA, "",
                           WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                           x, y, w, 28, p, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    SendMessage(t, TBM_SETRANGE, TRUE, MAKELONG(mn, mx));
    SendMessage(t, TBM_SETPAGESIZE, 0, (mx - mn) / 20 + 1);
    return t;
}
static HWND MkEdit(HWND p, int id, int x, int y, int w) {
    return CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                         x, y, w, 22, p, (HMENU)(INT_PTR)id, g_hInst, nullptr);
}
static int  TrackPos(HWND t) { return (int)SendMessage(t, TBM_GETPOS, 0, 0); }
static void SetTrack(HWND t, int v) { SendMessage(t, TBM_SETPOS, TRUE, v); }
static bool Checked(HWND c) { return SendMessage(c, BM_GETCHECK, 0, 0) == BST_CHECKED; }
static void SetCheck(HWND c, bool v) { SendMessage(c, BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0); }
static void SetLblF(HWND l, const char* fmt, int v) { char b[64]; wsprintfA(b, fmt, v); SetWindowTextA(l, b); }

// EnumChildWindows ile tum kontrollere font ata
static BOOL CALLBACK SetFontCb(HWND h, LPARAM f) {
    SendMessage(h, WM_SETFONT, (WPARAM)f, TRUE); return TRUE;
}

// ---- INI kalicilik ----
static void IniPath() {
    GetModuleFileNameA(nullptr, g_ini, MAX_PATH);
    char* s = strrchr(g_ini, '\\');
    if (s) strcpy(s + 1, "m2t2gui.ini"); else strcpy(g_ini, "m2t2gui.ini");
}
static void SaveIni() {
    if (!g_shm) return;
    char b[128];
    #define PUT(k,v) do{ wsprintfA(b,"%d",(int)(v)); WritePrivateProfileStringA("m2t2",k,b,g_ini);}while(0)
    PUT("range",   (int)g_shm->farmRange);
    PUT("delay",   g_shm->attackDelayMs);
    PUT("maxt",    g_shm->maxTargets);
    PUT("atype",   g_shm->attackType);
    PUT("hppct",   g_shm->hpPotPct);
    PUT("sppct",   g_shm->spPotPct);
    PUT("autofarm",g_shm->autoFarm);
    PUT("autopot", g_shm->autoPotion);
    PUT("verbose", g_shm->verbose);
    PUT("speed",   g_shm->speedHackOn);
    #undef PUT
    WritePrivateProfileStringA("m2t2", "name", g_shm->targetName, g_ini);
}
static void LoadIni(M2T2Shared* s) {
    s->farmRange     = (float)GetPrivateProfileIntA("m2t2", "range", (int)s->farmRange, g_ini);
    s->attackDelayMs = GetPrivateProfileIntA("m2t2", "delay",  s->attackDelayMs, g_ini);
    s->maxTargets    = GetPrivateProfileIntA("m2t2", "maxt",   s->maxTargets, g_ini);
    s->attackType    = GetPrivateProfileIntA("m2t2", "atype",  s->attackType, g_ini);
    s->hpPotPct      = GetPrivateProfileIntA("m2t2", "hppct",  s->hpPotPct, g_ini);
    s->spPotPct      = GetPrivateProfileIntA("m2t2", "sppct",  s->spPotPct, g_ini);
    s->autoFarm      = GetPrivateProfileIntA("m2t2", "autofarm", s->autoFarm, g_ini);
    s->autoPotion    = GetPrivateProfileIntA("m2t2", "autopot",  s->autoPotion, g_ini);
    s->verbose       = GetPrivateProfileIntA("m2t2", "verbose",  s->verbose, g_ini);
    s->speedHackOn   = GetPrivateProfileIntA("m2t2", "speed",    s->speedHackOn, g_ini);
    GetPrivateProfileStringA("m2t2", "name", s->targetName, s->targetName,
                             sizeof(s->targetName), g_ini);
}

// Kontrolleri shm'den doldur (baslangicta ve GUI DLL'e baglaninca bir kez)
static void LoadControls() {
    if (!g_shm) return;
    g_loading = true;
    SetCheck(hBot,   g_shm->botEnabled != 0);
    SetCheck(hMode,  g_shm->autoFarm == 0);     // isaretli = Target modu
    SetCheck(hPot,   g_shm->autoPotion != 0);
    SetCheck(hDebug, g_shm->verbose != 0);
    SetCheck(hSpeed, g_shm->speedHackOn != 0);
    SetTrack(hRange, (int)g_shm->farmRange);
    SetTrack(hDelay, g_shm->attackDelayMs);
    SetTrack(hMaxT,  g_shm->maxTargets);
    SetTrack(hAType, g_shm->attackType);
    SetTrack(hHp,    g_shm->hpPotPct);
    SetTrack(hSp,    g_shm->spPotPct);
    SetWindowTextA(hName, g_shm->targetName);
    g_loading = false;
}

// Kontrollerden shm'e yaz (deger etiketlerini de gunceller)
static void ApplyControls() {
    if (!g_shm || g_loading) return;
    g_shm->botEnabled  = Checked(hBot)   ? 1 : 0;
    g_shm->autoFarm    = Checked(hMode)  ? 0 : 1;   // isaretli = Target => autoFarm=0
    g_shm->autoPotion  = Checked(hPot)   ? 1 : 0;
    g_shm->verbose     = Checked(hDebug) ? 1 : 0;
    g_shm->speedHackOn = Checked(hSpeed) ? 1 : 0;
    g_shm->farmRange     = (float)TrackPos(hRange);
    g_shm->attackDelayMs = TrackPos(hDelay);
    g_shm->maxTargets    = TrackPos(hMaxT);
    g_shm->attackType    = TrackPos(hAType);
    g_shm->hpPotPct      = TrackPos(hHp);
    g_shm->spPotPct      = TrackPos(hSp);
    char nm[64] = {0};
    GetWindowTextA(hName, nm, sizeof(nm));
    strncpy(g_shm->targetName, nm, sizeof(g_shm->targetName) - 1);
    g_shm->targetName[sizeof(g_shm->targetName) - 1] = 0;

    SetLblF(hRangeV, "%d", (int)g_shm->farmRange);
    SetLblF(hDelayV, "%d ms", g_shm->attackDelayMs);
    SetLblF(hMaxTV,  "%d", g_shm->maxTargets);
    SetLblF(hATypeV, "%d", g_shm->attackType);
    SetLblF(hHpV,    "%%%d", g_shm->hpPotPct);
    SetLblF(hSpV,    "%%%d", g_shm->spPotPct);
    SaveIni();
}

// Durum etiketlerini guncelle (timer)
static void RefreshStatus() {
    static uint32_t lastHb = 0xFFFFFFFFu;
    static int stall = 0;
    if (!g_shm) { SetWindowTextA(hConn, "Paylasimli hafiza YOK"); return; }
    g_shm->guiHeartbeat++;

    bool alive;
    if (g_shm->dllHeartbeat != lastHb) { lastHb = g_shm->dllHeartbeat; stall = 0; alive = true; }
    else { alive = (++stall < 4); }

    char b[192];
    wsprintfA(b, "Oyun DLL: %s", alive ? "BAGLI" : "BAGLI DEGIL (inject bekleniyor)");
    SetWindowTextA(hConn, b);

    if (alive) {
        wsprintfA(b, "SendAttack: %s   RVA 0x%06X   filtre:%s",
                  g_shm->attackReady ? "HAZIR" : "yok",
                  g_shm->attackFnRva, g_shm->fastFilter ? "hizli" : "yavas");
        SetWindowTextA(hFn, b);
        wsprintfA(b, "HP: %d / %d", g_shm->curHp, g_shm->curMaxHp);       SetWindowTextA(hStHp, b);
        wsprintfA(b, "SP: %d / %d", g_shm->curSp, g_shm->curMaxSp);       SetWindowTextA(hStSp, b);
        wsprintfA(b, "Seviye: %d", g_shm->curLevel);                     SetWindowTextA(hStLvl, b);
        wsprintfA(b, "Son tarama: enum %d | menzilde %d | gonderilen %d",
                  g_shm->lastEnum, g_shm->lastInRange, g_shm->lastSent);  SetWindowTextA(hStFarm, b);
    } else {
        SetWindowTextA(hFn, "SendAttack: —");
        SetWindowTextA(hStHp, "HP: —");  SetWindowTextA(hStSp, "SP: —");
        SetWindowTextA(hStLvl, "Seviye: —");
        SetWindowTextA(hStFarm, "Son tarama: —");
    }
}

static void BuildUI(HWND w) {
    int x = 14, xv = 300, ww = 274;
    MkStatic(w, "— AYARLAR —", x, 8, 200, 18);
    hBot   = MkCheck(w, "BOT ACIK (INSERT ile ayni)",        ID_BOT,   x, 30,  260);
    hMode  = MkCheck(w, "Target modu (bos = menzil saldiri)", ID_MODE,  x, 56,  260);
    hPot   = MkCheck(w, "Otomatik iksir (F1/F2)",             ID_POT,   x, 82,  260);
    hDebug = MkCheck(w, "Debug log",                          ID_DEBUG, x, 108, 260);
    hSpeed = MkCheck(w, "Speedhack (yakinda)",                ID_SPEED, x, 134, 260);

    MkStatic(w, "Menzil (range)", x, 164, 180, 16);  hRangeV = MkStatic(w, "", xv, 164, 108, 16);
    hRange = MkTrack(w, ID_RANGE, x, 182, 390, 100, 5000);

    MkStatic(w, "Saldiri araligi", x, 214, 180, 16); hDelayV = MkStatic(w, "", xv, 214, 108, 16);
    hDelay = MkTrack(w, ID_DELAY, x, 232, 390, 50, 1000);

    MkStatic(w, "Max hedef (ayni anda)", x, 264, 200, 16); hMaxTV = MkStatic(w, "", xv, 264, 108, 16);
    hMaxT  = MkTrack(w, ID_MAXT, x, 282, 390, 1, 20);

    MkStatic(w, "Attack type (bType)", x, 314, 200, 16); hATypeV = MkStatic(w, "", xv, 314, 108, 16);
    hAType = MkTrack(w, ID_ATYPE, x, 332, 390, 0, 10);

    MkStatic(w, "HP iksir esigi", x, 364, 180, 16); hHpV = MkStatic(w, "", xv, 364, 108, 16);
    hHp    = MkTrack(w, ID_HPPCT, x, 382, 390, 0, 100);

    MkStatic(w, "SP iksir esigi", x, 414, 180, 16); hSpV = MkStatic(w, "", xv, 414, 108, 16);
    hSp    = MkTrack(w, ID_SPPCT, x, 432, 390, 0, 100);

    MkStatic(w, "Mob ismi filtresi (bos = hepsi)", x, 466, 260, 16);
    hName  = MkEdit(w, ID_NAME, x, 484, 390);

    MkStatic(w, "— DURUM —", x, 516, 200, 18);
    hConn   = MkStatic(w, "", x, 538, 404, 16);
    hFn     = MkStatic(w, "", x, 558, 404, 16);
    hStHp   = MkStatic(w, "", x, 578, 200, 16);
    hStSp   = MkStatic(w, "", x + 200, 578, 200, 16);
    hStLvl  = MkStatic(w, "", x, 598, 200, 16);
    hStFarm = MkStatic(w, "", x, 618, 404, 16);
}

static LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE:
        BuildUI(w);
        EnumChildWindows(w, SetFontCb, (LPARAM)GetStockObject(DEFAULT_GUI_FONT));
        LoadControls();
        ApplyControls();          // etiketleri doldur
        SetTimer(w, 1, 250, nullptr);
        return 0;
    case WM_HSCROLL:              // trackbar hareketi
        ApplyControls();
        return 0;
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED || HIWORD(wp) == EN_CHANGE) ApplyControls();
        return 0;
    case WM_TIMER:
        RefreshStatus();
        // DLL bot'u disaridan (INSERT) acip/kapatirsa kutuyu guncel tut
        if (g_shm) { g_loading = true; SetCheck(hBot, g_shm->botEnabled != 0); g_loading = false; }
        return 0;
    case WM_DESTROY:
        KillTimer(w, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(w, m, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    g_hInst = hInst;
    IniPath();

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    // Paylasimli hafizayi ac/olustur. Panel ayarlarin sahibidir:
    //  - yeni mapping ise once varsayilanlari kur,
    //  - her durumda kayitli INI ayarlarini uygula (botEnabled haric — hep kapali baslar).
    bool created = false;
    g_shm = M2T2_OpenShared(true, &g_shmH, &created);
    if (g_shm) {
        if (created || g_shm->magic != M2T2_SHM_MAGIC) M2T2_InitDefaults(g_shm);
        LoadIni(g_shm);
    }

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "M2T2GuiWnd";
    RegisterClassA(&wc);

    HWND w = CreateWindowA("M2T2GuiWnd", "M2T2 Bot — Ayar Penceresi",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 448, 684,
                           nullptr, nullptr, hInst, nullptr);
    if (!w) return 1;
    if (!g_shm) MessageBoxA(w, "Paylasimli hafiza acilamadi!", "M2T2", MB_ICONERROR);
    ShowWindow(w, nShow);
    UpdateWindow(w);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessage(w, &msg)) {   // TAB ile kontroller arasi gezinme
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    if (g_shm)  UnmapViewOfFile(g_shm);
    if (g_shmH) CloseHandle(g_shmH);
    return 0;
}

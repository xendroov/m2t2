#pragma once

// Oyun penceresinin WndProc'unu subclass eder. WndProc oyunun ANA THREAD'inde
// (mesaj dongusu) calisir — Python/oyun fonksiyonlarini oradan guvenle cagiririz.
// D3D'ye bagli degil; her Win32 penceresinde calisir.
namespace WndHook {
    bool Install();
    void Remove();
    bool IsInstalled();

    // Oyun penceresine tus gonder (WM_KEYDOWN+WM_KEYUP). Otomatik iksir vb. icin.
    void SendKey(int vk);
}

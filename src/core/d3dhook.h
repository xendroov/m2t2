#pragma once

// D3D9 EndScene hook'u. Oyun her kareyi cizerken EndScene ANA THREAD'de
// cagrilir. Bot kodunu oraya baglayinca, oyun fonksiyonlarini ana thread'de
// cagirmis oluruz — thread yarisi yok, crash yok.
namespace D3DHook {
    bool Install();   // EndScene hooklandi mi
    void Remove();
    bool IsInstalled();
}

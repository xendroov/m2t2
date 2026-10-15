#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>

// Calisma-zamani imza taramasi. metin2client VMProtect ile paketli oldugu icin
// disk uzerindeki .text bos; gercek kod SADECE bellekte acik durur. Bu yuzden
// fonksiyonlari string referansindan canli bellekte buluruz (surum bagimsiz).
namespace SigScan {

// Ana modul (metin2client.exe) image araligini dondurur.
void MainModuleRange(uintptr_t& base, uintptr_t& end);

// Committed + okunabilir tum bolgelerde (ana modul image icinde) birebir bayt
// dizisini arar. execOnly=true ise sadece calistirilabilir sayfalar taranir.
// Ilk eslesmeyi dondurur, yoksa 0.
uintptr_t FindBytes(const uint8_t* pat, size_t len, bool execOnly);

// Birebir C string (null HARIC) okunabilir bellekte arar.
uintptr_t FindString(const char* s);

// Tum eslesmeleri dondurur (cap kadar).
std::vector<uintptr_t> FindAllBytes(const uint8_t* pat, size_t len, bool execOnly, size_t cap);

// 'within' adresinden geriye yurur, en yakin fonksiyon prologunu (55 8B EC =
// push ebp; mov ebp,esp) bulur; onunde padding (CC) veya ret (C3 / C2 xx xx)
// olmali. Bulamazsa 0.
uintptr_t FindFunctionStart(uintptr_t within, size_t maxBack);

} // namespace SigScan

#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <tuple>
#include <optional>

// -----------------------------------------------------------------------
// Python C API wrapper — oyunun gömülü python27.dll'ini kullanır.
// Tüm oyun fonksiyonları PyCFunction imzasındadır:
//   PyObject* fn(PyObject* self, PyObject* args)
// -----------------------------------------------------------------------

// Python tip tanımları (python.h dahil etmeden minimal stub)
struct _object;
using PyObject = _object;

namespace PyAPI {

// Başlatma: python27.dll'den fonksiyon pointer'larını al
bool Init();

// GIL al/bırak
void AcquireGIL();
void ReleaseGIL();

// Düşük seviyeli çağrılar — args Python tuple'ı olmalı
PyObject* CallFn(uintptr_t fnAddr, PyObject* args);

// Yüksek seviyeli kolaylaştırıcılar
// Int argümanlı çağrı -> int sonuç döner, hata olursa -1
int  CallInt_Int(uintptr_t fn, int arg);
int  CallInt_Void(uintptr_t fn);
bool CallBool_Int(uintptr_t fn, int arg);
void CallVoid_Int(uintptr_t fn, int arg);
void CallVoid_Void(uintptr_t fn);
std::string CallStr_Int(uintptr_t fn, int arg);
std::string CallStr_Void(uintptr_t fn);

// (x, y, z) tuple dönen fonksiyon için
std::tuple<float,float,float> CallXYZ_Void(uintptr_t fn);
std::tuple<float,float,float> CallXYZ_Int(uintptr_t fn, int arg);

} // namespace PyAPI

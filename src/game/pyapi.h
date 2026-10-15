#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <tuple>

// Python header'larını sadece pyapi.cpp içinde include ediyoruz.
// Dışarıya PyObject* sızdırmamak için opak pointer kullan.
struct _object;
using PyObject = _object;

namespace PyAPI {

bool Init();

void AcquireGIL();
void ReleaseGIL();

PyObject* CallFn(uintptr_t fnAddr, PyObject* args);

int         CallInt_Int  (uintptr_t fn, int arg);
int         CallInt_Void (uintptr_t fn);
bool        CallBool_Int (uintptr_t fn, int arg);
void        CallVoid_Int (uintptr_t fn, int arg);
void        CallVoid_Void(uintptr_t fn);
std::string CallStr_Int  (uintptr_t fn, int arg);
std::string CallStr_Void (uintptr_t fn);

std::tuple<float,float,float> CallXYZ_Void(uintptr_t fn);
std::tuple<float,float,float> CallXYZ_Int (uintptr_t fn, int arg);

} // namespace PyAPI

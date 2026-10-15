#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

struct _object;
using PyObject = _object;

namespace PyAPI {

bool Init();
void ImportModules(); // Init() basarili olduktan sonra cagir

// sys.modules'taki TUM modul isimlerini dondurur (import KULLANMAZ, C API ile).
std::vector<std::string> ModuleNames();

// sys.modules[sysName] modulunu __main__'e mainName olarak kopyalar.
// Boylece EvalInt("player.GetStatus(...)") gibi ifadeler calisabilir.
bool InjectModule(const char* sysName, const char* mainName);

// Bir Python builtin-fonksiyonun GERCEK C adresini dondurur.
// expr ornegi: "chrmgr.IsDead" -> o fonksiyonun bellekteki adresi.
// PyCFunctionObject->m_ml->ml_meth okunur. VMProtect'te bile calisir
// cunku bellekteki acilmis kodu isaret eder.
uintptr_t ResolveCFunc(const char* expr);

// Python expression değerlendir: "player.GetStatus(0)" → int
int         EvalInt(const char* expr);
bool        EvalBool(const char* expr);
float       EvalFloat(const char* expr);
std::string EvalStr(const char* expr);

// Python deyimi çalıştır: "net.SendOnClickPacket(12345)"
void Exec(const char* code);

// (x, y, z) tuple dönen expression
std::tuple<float,float,float> EvalXYZ(const char* expr);

// Düşük seviyeli — direkt adres çağrısı (adres doğrulandıysa kullan)
void AcquireGIL();
void ReleaseGIL();
PyObject* CallFn(uintptr_t fnAddr, PyObject* args);
int  CallInt_Int  (uintptr_t fn, int arg);
int  CallInt_Void (uintptr_t fn);
bool CallBool_Int (uintptr_t fn, int arg);
void CallVoid_Int (uintptr_t fn, int arg);
void CallVoid_Void(uintptr_t fn);
std::string CallStr_Int(uintptr_t fn, int arg);
std::tuple<float,float,float> CallXYZ_Void(uintptr_t fn);
std::tuple<float,float,float> CallXYZ_Int (uintptr_t fn, int arg);

} // namespace PyAPI

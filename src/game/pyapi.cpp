#include "pyapi.h"
#include "../core/console.h"

// Python 2.7 header'ları C++17'de kaldırılan "register" anahtar kelimesini kullanır.
// Derleyici hatasını önlemek için önce boş tanımla.
#define register
#include <Python.h>
#undef register

namespace PyAPI {

// python27.dll'den yüklenen fonksiyon pointer'ları
// İsimler GetProcAddress'e verilen string ile birebir eşleşmeli
using tPy_BuildValue     = PyObject*(__cdecl*)(const char*, ...);
using tPyInt_AsLong      = long(__cdecl*)(PyObject*);
using tPyFloat_AsDouble  = double(__cdecl*)(PyObject*);
using tPyString_AsString = const char*(__cdecl*)(PyObject*);
using tPyTuple_GetItem   = PyObject*(__cdecl*)(PyObject*, Py_ssize_t);
using tPy_DecRef         = void(__cdecl*)(PyObject*);
using tPyGILState_Ensure = PyGILState_STATE(__cdecl*)();
using tPyGILState_Release= void(__cdecl*)(PyGILState_STATE);
using tPyErr_Clear       = void(__cdecl*)();
using tPyCFunction       = PyObject*(__cdecl*)(PyObject*, PyObject*);

static HMODULE              g_dll        = nullptr;
static tPy_BuildValue       g_Build      = nullptr;
static tPyInt_AsLong        g_IntAsLong  = nullptr;
static tPyFloat_AsDouble    g_FltAsDbl   = nullptr;
static tPyString_AsString   g_StrAsStr   = nullptr;
static tPyTuple_GetItem     g_TupGet     = nullptr;
static tPy_DecRef           g_DecRef     = nullptr;
static tPyGILState_Ensure   g_GILEnsure  = nullptr;
static tPyGILState_Release  g_GILRelease = nullptr;
static tPyErr_Clear         g_ErrClear   = nullptr;
static PyGILState_STATE     g_gilState{};

#define LOAD(var, sym) \
    var = (decltype(var))GetProcAddress(g_dll, #sym); \
    if (!var) { Console::Log("[PyAPI] '" #sym "' yuklenemedi"); return false; }

bool Init() {
    g_dll = GetModuleHandleA("python27.dll");
    if (!g_dll) {
        Console::Log("[PyAPI] python27.dll bulunamadi — Python 2.7 (32-bit) kurulu mu?");
        return false;
    }

    LOAD(g_Build,      Py_BuildValue)
    LOAD(g_IntAsLong,  PyInt_AsLong)
    LOAD(g_FltAsDbl,   PyFloat_AsDouble)
    LOAD(g_StrAsStr,   PyString_AsString)
    LOAD(g_TupGet,     PyTuple_GetItem)
    LOAD(g_DecRef,     Py_DecRef)
    LOAD(g_GILEnsure,  PyGILState_Ensure)
    LOAD(g_GILRelease, PyGILState_Release)
    LOAD(g_ErrClear,   PyErr_Clear)

    Console::Log("[PyAPI] Hazir — python27 @ 0x%p", (void*)g_dll);
    return true;
}
#undef LOAD

void AcquireGIL()  { if (g_GILEnsure)  g_gilState = g_GILEnsure(); }
void ReleaseGIL()  { if (g_GILRelease) g_GILRelease(g_gilState); }

PyObject* CallFn(uintptr_t addr, PyObject* args) {
    if (!addr) return nullptr;
    auto fn  = reinterpret_cast<tPyCFunction>(addr);
    auto ret = fn(nullptr, args);
    if (!ret && g_ErrClear) g_ErrClear();
    return ret;
}

// --- Yüksek seviyeli sarmalayıcılar ---

int CallInt_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    auto* args = g_Build("(i)", arg);
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    int r = ret ? (int)g_IntAsLong(ret) : -1;
    if (ret) g_DecRef(ret);
    ReleaseGIL();
    return r;
}

int CallInt_Void(uintptr_t fn) {
    AcquireGIL();
    auto* args = g_Build("()");
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    int r = ret ? (int)g_IntAsLong(ret) : -1;
    if (ret) g_DecRef(ret);
    ReleaseGIL();
    return r;
}

bool CallBool_Int(uintptr_t fn, int arg) { return CallInt_Int(fn, arg) != 0; }

void CallVoid_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    auto* args = g_Build("(i)", arg);
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    if (ret)  g_DecRef(ret);
    ReleaseGIL();
}

void CallVoid_Void(uintptr_t fn) {
    AcquireGIL();
    auto* args = g_Build("()");
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    if (ret)  g_DecRef(ret);
    ReleaseGIL();
}

std::string CallStr_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    auto* args = g_Build("(i)", arg);
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    std::string r;
    if (ret) { const char* s = g_StrAsStr(ret); if (s) r = s; g_DecRef(ret); }
    ReleaseGIL();
    return r;
}

std::string CallStr_Void(uintptr_t fn) {
    AcquireGIL();
    auto* args = g_Build("()");
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    std::string r;
    if (ret) { const char* s = g_StrAsStr(ret); if (s) r = s; g_DecRef(ret); }
    ReleaseGIL();
    return r;
}

std::tuple<float,float,float> CallXYZ_Void(uintptr_t fn) {
    AcquireGIL();
    auto* args = g_Build("()");
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    float x = 0, y = 0, z = 0;
    if (ret) {
        auto* px = g_TupGet(ret, 0);
        auto* py = g_TupGet(ret, 1);
        auto* pz = g_TupGet(ret, 2);
        if (px) x = (float)g_FltAsDbl(px);
        if (py) y = (float)g_FltAsDbl(py);
        if (pz) z = (float)g_FltAsDbl(pz);
        g_DecRef(ret);
    }
    ReleaseGIL();
    return { x, y, z };
}

std::tuple<float,float,float> CallXYZ_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    auto* args = g_Build("(i)", arg);
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    float x = 0, y = 0, z = 0;
    if (ret) {
        auto* px = g_TupGet(ret, 0);
        auto* py = g_TupGet(ret, 1);
        auto* pz = g_TupGet(ret, 2);
        if (px) x = (float)g_FltAsDbl(px);
        if (py) y = (float)g_FltAsDbl(py);
        if (pz) z = (float)g_FltAsDbl(pz);
        g_DecRef(ret);
    }
    ReleaseGIL();
    return { x, y, z };
}

} // namespace PyAPI

#include "pyapi.h"
#include "../core/console.h"

// Python.h dahil ETMİYORUZ — her şeyi GetProcAddress ile runtime'da yüklüyoruz.
// Bu sayede python27.lib gerekmez, sadece python27.dll yeterlidir.

namespace PyAPI {

// PyObject forward declaration (pyapi.h'da zaten var: struct _object)
// PyGILState_STATE = int (python27 ABI'sinde sabit)
using PyGILState_STATE = int;

// Fonksiyon tipleri
using tPy_BuildValue     = void*(__cdecl*)(const char*, ...);
using tPyInt_AsLong      = long(__cdecl*)(void*);
using tPyFloat_AsDouble  = double(__cdecl*)(void*);
using tPyString_AsString = const char*(__cdecl*)(void*);
using tPyTuple_GetItem   = void*(__cdecl*)(void*, int);
using tPy_DecRef         = void(__cdecl*)(void*);
using tPyGILState_Ensure = PyGILState_STATE(__cdecl*)();
using tPyGILState_Release= void(__cdecl*)(PyGILState_STATE);
using tPyErr_Clear       = void(__cdecl*)();
using tPyCFunction       = void*(__cdecl*)(void*, void*);

static HMODULE             g_dll        = nullptr;
static tPy_BuildValue      g_Build      = nullptr;
static tPyInt_AsLong       g_IntAsLong  = nullptr;
static tPyFloat_AsDouble   g_FltAsDbl   = nullptr;
static tPyString_AsString  g_StrAsStr   = nullptr;
static tPyTuple_GetItem    g_TupGet     = nullptr;
static tPy_DecRef          g_DecRef     = nullptr;
static tPyGILState_Ensure  g_GILEnsure  = nullptr;
static tPyGILState_Release g_GILRelease = nullptr;
static tPyErr_Clear        g_ErrClear   = nullptr;
static PyGILState_STATE    g_gilState   = 0;

#define LOAD(var, sym) \
    var = reinterpret_cast<decltype(var)>(GetProcAddress(g_dll, #sym)); \
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
    auto ret = reinterpret_cast<PyObject*>(fn(nullptr, args));
    if (!ret && g_ErrClear) g_ErrClear();
    return ret;
}

int CallInt_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    auto* args = reinterpret_cast<PyObject*>(g_Build("(i)", arg));
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    int r = ret ? (int)g_IntAsLong(ret) : -1;
    if (ret) g_DecRef(ret);
    ReleaseGIL();
    return r;
}

int CallInt_Void(uintptr_t fn) {
    AcquireGIL();
    auto* args = reinterpret_cast<PyObject*>(g_Build("()"));
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
    auto* args = reinterpret_cast<PyObject*>(g_Build("(i)", arg));
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    if (ret)  g_DecRef(ret);
    ReleaseGIL();
}

void CallVoid_Void(uintptr_t fn) {
    AcquireGIL();
    auto* args = reinterpret_cast<PyObject*>(g_Build("()"));
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    if (ret)  g_DecRef(ret);
    ReleaseGIL();
}

std::string CallStr_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    auto* args = reinterpret_cast<PyObject*>(g_Build("(i)", arg));
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    std::string r;
    if (ret) { const char* s = g_StrAsStr(ret); if (s) r = s; g_DecRef(ret); }
    ReleaseGIL();
    return r;
}

std::string CallStr_Void(uintptr_t fn) {
    AcquireGIL();
    auto* args = reinterpret_cast<PyObject*>(g_Build("()"));
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    std::string r;
    if (ret) { const char* s = g_StrAsStr(ret); if (s) r = s; g_DecRef(ret); }
    ReleaseGIL();
    return r;
}

std::tuple<float,float,float> CallXYZ_Void(uintptr_t fn) {
    AcquireGIL();
    auto* args = reinterpret_cast<PyObject*>(g_Build("()"));
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    float x = 0, y = 0, z = 0;
    if (ret) {
        auto* px = reinterpret_cast<PyObject*>(g_TupGet(ret, 0));
        auto* py = reinterpret_cast<PyObject*>(g_TupGet(ret, 1));
        auto* pz = reinterpret_cast<PyObject*>(g_TupGet(ret, 2));
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
    auto* args = reinterpret_cast<PyObject*>(g_Build("(i)", arg));
    auto* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    float x = 0, y = 0, z = 0;
    if (ret) {
        auto* px = reinterpret_cast<PyObject*>(g_TupGet(ret, 0));
        auto* py = reinterpret_cast<PyObject*>(g_TupGet(ret, 1));
        auto* pz = reinterpret_cast<PyObject*>(g_TupGet(ret, 2));
        if (px) x = (float)g_FltAsDbl(px);
        if (py) y = (float)g_FltAsDbl(py);
        if (pz) z = (float)g_FltAsDbl(pz);
        g_DecRef(ret);
    }
    ReleaseGIL();
    return { x, y, z };
}

} // namespace PyAPI

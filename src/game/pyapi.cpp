#include "pyapi.h"
#include "../core/console.h"
#include <Python.h>   // python27.dll headers — CMake'de Python_ROOT_DIR ayarla

// -----------------------------------------------------------------------
// Metin2 client python27.dll'ini dinamik olarak yükler.
// Bu sayede projeye Python SDK eklemek gerekmez.
// -----------------------------------------------------------------------

namespace PyAPI {

// Dinamik yüklenen python27 fonksiyon pointer'ları
using Py_BuildValueFn    = PyObject*(__cdecl*)(const char*, ...);
using PyArg_ParseTupleFn = int(__cdecl*)(PyObject*, const char*, ...);
using PyInt_AsLongFn     = long(__cdecl*)(PyObject*);
using PyFloat_AsDoubleFn = double(__cdecl*)(PyObject*);
using PyString_AsStringFn= const char*(__cdecl*)(PyObject*);
using PyTuple_GetItemFn  = PyObject*(__cdecl*)(PyObject*, int);
using PyBool_FromLongFn  = PyObject*(__cdecl*)(long);
using Py_DecRefFn        = void(__cdecl*)(PyObject*);
using Py_IncRefFn        = void(__cdecl*)(PyObject*);
using PyGILState_EnsureFn  = int(__cdecl*)();
using PyGILState_ReleaseFn = void(__cdecl*)(int);
using PyErr_ClearFn      = void(__cdecl*)();

static HMODULE                 g_pyDll     = nullptr;
static Py_BuildValueFn         g_BuildVal  = nullptr;
static PyArg_ParseTupleFn      g_ParseTup  = nullptr;
static PyInt_AsLongFn          g_IntAsLong = nullptr;
static PyFloat_AsDoubleFn      g_FltAsDbl  = nullptr;
static PyString_AsStringFn     g_StrAsStr  = nullptr;
static PyTuple_GetItemFn       g_TupGet    = nullptr;
static Py_DecRefFn             g_DecRef    = nullptr;
static PyGILState_EnsureFn     g_GILEnsure = nullptr;
static PyGILState_ReleaseFn    g_GILRelease= nullptr;
static PyErr_ClearFn           g_ErrClear  = nullptr;

// PyCFunction tipi
using PyCFunc = PyObject*(__cdecl*)(PyObject*, PyObject*);

static int g_gilState = 0;

bool Init() {
    g_pyDll = GetModuleHandleA("python27.dll");
    if (!g_pyDll) {
        Console::Log("[PyAPI] python27.dll bulunamadi!");
        return false;
    }

#define LOAD(name, type) g_##name = (type##Fn)GetProcAddress(g_pyDll, #name); \
    if(!g_##name) { Console::Log("[PyAPI] " #name " yuklenemedi"); return false; }

    LOAD(Py_BuildValue,    Py_BuildValue)
    LOAD(PyArg_ParseTuple, PyArg_ParseTuple)
    LOAD(PyInt_AsLong,     PyInt_AsLong)
    LOAD(PyFloat_AsDouble, PyFloat_AsDouble)
    LOAD(PyString_AsString,PyString_AsString)
    LOAD(PyTuple_GetItem,  PyTuple_GetItem)
    LOAD(Py_DecRef,        Py_DecRef)
    LOAD(PyGILState_Ensure, PyGILState_Ensure)
    LOAD(PyGILState_Release, PyGILState_Release)
    LOAD(PyErr_Clear,       PyErr_Clear)
#undef LOAD

    Console::Log("[PyAPI] Hazir — python27.dll @ 0x%p", g_pyDll);
    return true;
}

void AcquireGIL() {
    if (g_GILEnsure) g_gilState = g_GILEnsure();
}

void ReleaseGIL() {
    if (g_GILRelease) g_GILRelease(g_gilState);
}

// Temel çağrı: (fn)(nullptr, args) — Python modül fonksiyonları self=nullptr alır
PyObject* CallFn(uintptr_t fnAddr, PyObject* args) {
    if (!fnAddr) return nullptr;
    auto fn = reinterpret_cast<PyCFunc>(fnAddr);
    PyObject* ret = fn(nullptr, args);
    if (!ret && g_ErrClear) g_ErrClear();
    return ret;
}

int CallInt_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    PyObject* args = g_BuildVal("(i)", arg);
    PyObject* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    int result = ret ? (int)g_IntAsLong(ret) : -1;
    if (ret) g_DecRef(ret);
    ReleaseGIL();
    return result;
}

int CallInt_Void(uintptr_t fn) {
    AcquireGIL();
    PyObject* args = g_BuildVal("()");
    PyObject* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    int result = ret ? (int)g_IntAsLong(ret) : -1;
    if (ret) g_DecRef(ret);
    ReleaseGIL();
    return result;
}

bool CallBool_Int(uintptr_t fn, int arg) {
    return CallInt_Int(fn, arg) != 0;
}

void CallVoid_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    PyObject* args = g_BuildVal("(i)", arg);
    PyObject* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    if (ret)  g_DecRef(ret);
    ReleaseGIL();
}

void CallVoid_Void(uintptr_t fn) {
    AcquireGIL();
    PyObject* args = g_BuildVal("()");
    PyObject* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    if (ret)  g_DecRef(ret);
    ReleaseGIL();
}

std::string CallStr_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    PyObject* args = g_BuildVal("(i)", arg);
    PyObject* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    std::string result;
    if (ret) { auto s = g_StrAsStr(ret); if (s) result = s; g_DecRef(ret); }
    ReleaseGIL();
    return result;
}

std::string CallStr_Void(uintptr_t fn) {
    AcquireGIL();
    PyObject* args = g_BuildVal("()");
    PyObject* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);
    std::string result;
    if (ret) { auto s = g_StrAsStr(ret); if (s) result = s; g_DecRef(ret); }
    ReleaseGIL();
    return result;
}

std::tuple<float,float,float> CallXYZ_Void(uintptr_t fn) {
    AcquireGIL();
    PyObject* args = g_BuildVal("()");
    PyObject* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);

    float x = 0, y = 0, z = 0;
    if (ret) {
        PyObject* px = g_TupGet(ret, 0);
        PyObject* py = g_TupGet(ret, 1);
        PyObject* pz = g_TupGet(ret, 2);
        if (px) x = (float)g_FltAsDbl(px);
        if (py) y = (float)g_FltAsDbl(py);
        if (pz) z = (float)g_FltAsDbl(pz);
        g_DecRef(ret);
    }
    ReleaseGIL();
    return {x, y, z};
}

std::tuple<float,float,float> CallXYZ_Int(uintptr_t fn, int arg) {
    AcquireGIL();
    PyObject* args = g_BuildVal("(i)", arg);
    PyObject* ret  = CallFn(fn, args);
    if (args) g_DecRef(args);

    float x = 0, y = 0, z = 0;
    if (ret) {
        PyObject* px = g_TupGet(ret, 0);
        PyObject* py = g_TupGet(ret, 1);
        PyObject* pz = g_TupGet(ret, 2);
        if (px) x = (float)g_FltAsDbl(px);
        if (py) y = (float)g_FltAsDbl(py);
        if (pz) z = (float)g_FltAsDbl(pz);
        g_DecRef(ret);
    }
    ReleaseGIL();
    return {x, y, z};
}

} // namespace PyAPI

#include "pyapi.h"
#include "../core/console.h"

// Python.h dahil ETMİYORUZ — her şeyi GetProcAddress ile runtime'da yüklüyoruz.
// Bu sayede python27.lib gerekmez, sadece python27.dll yeterlidir.

namespace PyAPI {

// PyObject forward declaration (pyapi.h'da zaten var: struct _object)
// PyGILState_STATE = int (python27 ABI'sinde sabit)
using PyGILState_STATE = int;

// g_modulesImported: Init() içinde player/net/chr/chrmgr/nonplayer'ı __main__'e import eder
static bool g_modulesImported = false;

// Py_eval_input / Py_file_input compile constants (Python 2.7 sabit değerleri)
static constexpr int PY_EVAL_INPUT   = 258;
static constexpr int PY_FILE_INPUT   = 257;
static constexpr int PY_SINGLE_INPUT = 256;

// Fonksiyon tipleri
using tPy_BuildValue        = void*(__cdecl*)(const char*, ...);
using tPyInt_AsLong         = long(__cdecl*)(void*);
using tPyFloat_AsDouble     = double(__cdecl*)(void*);
using tPyString_AsString    = const char*(__cdecl*)(void*);
using tPyTuple_GetItem      = void*(__cdecl*)(void*, int);
using tPy_DecRef            = void(__cdecl*)(void*);
using tPyGILState_Ensure    = PyGILState_STATE(__cdecl*)();
using tPyGILState_Release   = void(__cdecl*)(PyGILState_STATE);
using tPyErr_Clear          = void(__cdecl*)();
using tPyCFunction          = void*(__cdecl*)(void*, void*);
using tPyRun_String         = void*(__cdecl*)(const char*, int, void*, void*);
using tPyImport_AddModule   = void*(__cdecl*)(const char*);
using tPyModule_GetDict     = void*(__cdecl*)(void*);
using tPyErr_Occurred       = void*(__cdecl*)();
using tPyErr_Print          = void(__cdecl*)();
using tPyImport_GetModuleDict = void*(__cdecl*)();
using tPyDict_Keys          = void*(__cdecl*)(void*);
using tPyDict_GetItemString = void*(__cdecl*)(void*, const char*);
using tPyDict_SetItemString = int(__cdecl*)(void*, const char*, void*);
using tPyList_Size          = int(__cdecl*)(void*);
using tPyList_GetItem       = void*(__cdecl*)(void*, int);

static HMODULE              g_dll         = nullptr;
static tPy_BuildValue       g_Build       = nullptr;
static tPyInt_AsLong        g_IntAsLong   = nullptr;
static tPyFloat_AsDouble    g_FltAsDbl    = nullptr;
static tPyString_AsString   g_StrAsStr    = nullptr;
static tPyTuple_GetItem     g_TupGet      = nullptr;
static tPy_DecRef           g_DecRef      = nullptr;
static tPyGILState_Ensure   g_GILEnsure   = nullptr;
static tPyGILState_Release  g_GILRelease  = nullptr;
static tPyErr_Clear         g_ErrClear    = nullptr;
static tPyRun_String        g_RunStr      = nullptr;
static tPyImport_AddModule  g_AddModule   = nullptr;
static tPyModule_GetDict    g_ModGetDict  = nullptr;
static tPyErr_Occurred      g_ErrOccurred = nullptr;
static tPyErr_Print         g_ErrPrint    = nullptr;
static tPyImport_GetModuleDict g_GetModuleDict = nullptr;
static tPyDict_Keys         g_DictKeys    = nullptr;
static tPyDict_GetItemString g_DictGetStr = nullptr;
static tPyDict_SetItemString g_DictSetStr = nullptr;
static tPyList_Size         g_ListSize    = nullptr;
static tPyList_GetItem      g_ListGetItem = nullptr;
static PyGILState_STATE     g_gilState    = 0;

#define LOAD(var, sym) \
    var = reinterpret_cast<decltype(var)>(GetProcAddress(g_dll, #sym)); \
    if (!var) { Console::Log("[PyAPI] '" #sym "' yuklenemedi"); return false; }

bool Init() {
    g_dll = GetModuleHandleA("python27.dll");
    if (!g_dll) {
        Console::Log("[PyAPI] python27.dll bulunamadi — Python 2.7 (32-bit) kurulu mu?");
        return false;
    }
    LOAD(g_Build,       Py_BuildValue)
    LOAD(g_IntAsLong,   PyInt_AsLong)
    LOAD(g_FltAsDbl,    PyFloat_AsDouble)
    LOAD(g_StrAsStr,    PyString_AsString)
    LOAD(g_TupGet,      PyTuple_GetItem)
    LOAD(g_DecRef,      Py_DecRef)
    LOAD(g_GILEnsure,   PyGILState_Ensure)
    LOAD(g_GILRelease,  PyGILState_Release)
    LOAD(g_ErrClear,    PyErr_Clear)
    LOAD(g_RunStr,      PyRun_String)
    LOAD(g_AddModule,   PyImport_AddModule)
    LOAD(g_ModGetDict,  PyModule_GetDict)
    LOAD(g_ErrOccurred, PyErr_Occurred)
    LOAD(g_ErrPrint,    PyErr_Print)
    LOAD(g_GetModuleDict, PyImport_GetModuleDict)
    LOAD(g_DictKeys,    PyDict_Keys)
    LOAD(g_DictGetStr,  PyDict_GetItemString)
    LOAD(g_DictSetStr,  PyDict_SetItemString)
    LOAD(g_ListSize,    PyList_Size)
    LOAD(g_ListGetItem, PyList_GetItem)
    Console::Log("[PyAPI] Hazir — python27 @ 0x%p", (void*)g_dll);
    return true;
}
#undef LOAD

// __main__ modülünün globals dict'ini döner (borrowed ref — DecRef etme)
static void* MainGlobals() {
    if (!g_AddModule || !g_ModGetDict) return nullptr;
    void* mod = g_AddModule("__main__");
    return mod ? g_ModGetDict(mod) : nullptr;
}

// PyRun_String ile expression değerlendir, sonucu döner (new ref — caller DecRef eder)
static void* EvalExpr(const char* expr) {
    void* g = MainGlobals();
    if (!g || !g_RunStr) return nullptr;
    void* result = g_RunStr(expr, PY_EVAL_INPUT, g, g);
    if (!result && g_ErrOccurred && g_ErrOccurred()) {
        // PyErr_Print() oyunun sys.stderr'ına yazar — başka thread'den crash verebilir.
        // Sadece temizle, sessizce geç.
        if (g_ErrClear) g_ErrClear();
    }
    return result;
}

// --- Yüksek seviyeli Eval fonksiyonları ---

int EvalInt(const char* expr) {
    AcquireGIL();
    void* r = EvalExpr(expr);
    int val = r ? (int)g_IntAsLong(r) : 0;
    if (r) g_DecRef(r);
    ReleaseGIL();
    return val;
}

bool EvalBool(const char* expr) {
    return EvalInt(expr) != 0;
}

float EvalFloat(const char* expr) {
    AcquireGIL();
    void* r = EvalExpr(expr);
    float val = r ? (float)g_FltAsDbl(r) : 0.f;
    if (r) g_DecRef(r);
    ReleaseGIL();
    return val;
}

std::string EvalStr(const char* expr) {
    AcquireGIL();
    void* r = EvalExpr(expr);
    std::string val;
    if (r) {
        const char* s = g_StrAsStr(r);
        if (s) val = s;
        g_DecRef(r);
    }
    ReleaseGIL();
    return val;
}

void Exec(const char* code) {
    AcquireGIL();
    void* g = MainGlobals();
    if (g && g_RunStr) {
        void* r = g_RunStr(code, PY_FILE_INPUT, g, g);
        if (!r && g_ErrOccurred && g_ErrOccurred()) {
            if (g_ErrClear) g_ErrClear();
        }
        if (r) g_DecRef(r);
    }
    ReleaseGIL();
}

std::tuple<float,float,float> EvalXYZ(const char* expr) {
    AcquireGIL();
    void* r = EvalExpr(expr);
    float x = 0, y = 0, z = 0;
    if (r) {
        void* px = g_TupGet(r, 0);
        void* py = g_TupGet(r, 1);
        void* pz = g_TupGet(r, 2);
        if (px) x = (float)g_FltAsDbl(px);
        if (py) y = (float)g_FltAsDbl(py);
        if (pz) z = (float)g_FltAsDbl(pz);
        g_DecRef(r);
    }
    ReleaseGIL();
    return { x, y, z };
}

void AcquireGIL()  { if (g_GILEnsure)  g_gilState = g_GILEnsure(); }
void ReleaseGIL()  { if (g_GILRelease) g_GILRelease(g_gilState); }

// Init() basarili olduktan sonra cagrilmali — oyun modullerini __main__'e import eder.
// Boylece EvalInt("player.GetStatus(player.HP)") gibi ifadeler calisir.
void ImportModules() {
    AcquireGIL();
    void* g = MainGlobals();
    if (g && g_RunStr) {
        const char* imports =
            "import player\n"
            "import net\n"
            "import chr\n"
            "import chrmgr\n"
            "import nonplayer\n";
        void* r = g_RunStr(imports, PY_FILE_INPUT, g, g);
        if (r) {
            g_DecRef(r);
            g_modulesImported = true;
            Console::Log("[PyAPI] Moduller import edildi: player net chr chrmgr nonplayer");
        } else {
            if (g_ErrClear) g_ErrClear();
            Console::Log("[PyAPI] UYARI: Modul import basarisiz");
        }
    }
    ReleaseGIL();
}

// sys.modules'taki tum modul isimlerini dondurur — import KULLANMAZ.
// PyImport_GetModuleDict dogrudan sys.modules dict'ini verir (borrowed).
std::vector<std::string> ModuleNames() {
    std::vector<std::string> out;
    AcquireGIL();
    void* modules = g_GetModuleDict ? g_GetModuleDict() : nullptr;
    if (modules && g_DictKeys && g_ListSize && g_ListGetItem && g_StrAsStr) {
        void* keys = g_DictKeys(modules); // new ref
        if (keys) {
            int n = g_ListSize(keys);
            for (int i = 0; i < n; ++i) {
                void* k = g_ListGetItem(keys, i); // borrowed
                if (k) {
                    const char* s = g_StrAsStr(k);
                    if (s) out.push_back(s);
                }
            }
            g_DecRef(keys);
        }
    }
    ReleaseGIL();
    return out;
}

// sys.modules[sysName] → __main__[mainName]. import lock kullanmaz.
bool InjectModule(const char* sysName, const char* mainName) {
    bool ok = false;
    AcquireGIL();
    void* modules = g_GetModuleDict ? g_GetModuleDict() : nullptr;
    void* mainDict = MainGlobals();
    if (modules && mainDict && g_DictGetStr && g_DictSetStr) {
        void* mod = g_DictGetStr(modules, sysName); // borrowed
        if (mod) {
            g_DictSetStr(mainDict, mainName, mod);
            ok = true;
        }
    }
    ReleaseGIL();
    return ok;
}

// Python builtin-fonksiyonun gercek C adresini cozer.
// PyCFunctionObject (32-bit, release build):
//   offset 0: ob_refcnt (4)
//   offset 4: ob_type   (4)
//   offset 8: m_ml      -> PyMethodDef*
// PyMethodDef:
//   offset 0: ml_name
//   offset 4: ml_meth   -> gercek C fonksiyon adresi
uintptr_t ResolveCFunc(const char* expr) {
    uintptr_t result = 0;
    AcquireGIL();
    void* obj = EvalExpr(expr); // new ref — fonksiyon objesi
    if (obj) {
        uintptr_t m_ml = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(obj) + 8);
        if (m_ml) result = *reinterpret_cast<uintptr_t*>(m_ml + 4);
        g_DecRef(obj);
    }
    ReleaseGIL();
    return result;
}

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

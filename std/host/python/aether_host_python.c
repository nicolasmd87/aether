// aether_host_python.c — Embedded Python Language Host Module
//
// Embeds CPython in the Aether process. When run_sandboxed is called,
// installs the Aether sandbox checker so Python's libc calls are
// intercepted and checked against the grant list.

#include "aether_host_python.h"
#include "../../../runtime/aether_sandbox.h"
#include "../../../runtime/aether_shared_map.h"

// Forward declarations for context stack (compiler-generated, linked in)
extern void* _aether_ctx_stack[];
extern int _aether_ctx_depth;

#ifdef AETHER_HAS_PYTHON
#include <Python.h>
#include <stdlib.h>
#include <string.h>

static int python_initialized = 0;

// Permission checker that reads from the Aether context stack
// (same logic as the compiler-generated _aether_sandbox_check_impl)
extern int list_size(void*);
extern void* list_get(void*, int);

static int pattern_match(const char* pat, const char* resource) {
    int plen = strlen(pat);
    int rlen = strlen(resource);
    if (plen == 1 && pat[0] == '*') return 1;
    if (plen > 1 && pat[plen-1] == '*') {
        if (strncmp(pat, resource, plen-1) == 0) return 1;
    }
    if (plen > 1 && pat[0] == '*') {
        int slen = plen - 1;
        if (rlen >= slen && strcmp(resource + rlen - slen, pat + 1) == 0) return 1;
    }
    return strcmp(pat, resource) == 0;
}

static int perms_allow(void* ctx, const char* category, const char* resource) {
    if (!ctx) return 1;
    int n = list_size(ctx);
    if (n == 0) return 0;
    for (int i = 0; i < n; i += 2) {
        const char* cat = (const char*)list_get(ctx, i);
        const char* pat = (const char*)list_get(ctx, i + 1);
        if (!cat || !pat) continue;
        if (cat[0] == '*' && pat[0] == '*') return 1;
        if (strcmp(cat, category) == 0 && pattern_match(pat, resource)) return 1;
    }
    return 0;
}

static int host_python_checker(const char* category, const char* resource) {
    if (_aether_ctx_depth <= 0) return 1;
    for (int level = 0; level < _aether_ctx_depth; level++) {
        if (!perms_allow(_aether_ctx_stack[level], category, resource)) return 0;
    }
    return 1;
}

int python_init(void) {
    if (python_initialized) return 0;
    Py_Initialize();
    python_initialized = 1;
    return 0;
}

void python_finalize(void) {
    if (python_initialized) {
        Py_Finalize();
        python_initialized = 0;
    }
}

int python_run(const char* code) {
    if (!code) return -1;
    python_init();
    return PyRun_SimpleString(code);
}

int python_run_sandboxed(void* perms, const char* code) {
    if (!code) return -1;
    python_init();

    // Push perms onto context stack and install checker
    _aether_ctx_stack[_aether_ctx_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = host_python_checker;

    int result = PyRun_SimpleString(code);

    // Restore
    _aether_sandbox_checker = prev;
    _aether_ctx_depth--;

    return result;
}

// --- Shared map bindings for Python ---

static uint64_t py_current_map_token = 0;

// Python callable: aether_map_get(key) → str or None
static PyObject* py_aether_map_get(PyObject* self, PyObject* args) {
    (void)self;
    const char* key;
    if (!PyArg_ParseTuple(args, "s", &key)) return NULL;
    const char* val = aether_shared_map_get_by_token(py_current_map_token, key);
    if (val) return PyUnicode_FromString(val);
    Py_RETURN_NONE;
}

// Python callable: aether_map_put(key, value)
static PyObject* py_aether_map_put(PyObject* self, PyObject* args) {
    (void)self;
    const char* key;
    const char* value;
    if (!PyArg_ParseTuple(args, "ss", &key, &value)) return NULL;
    aether_shared_map_put_by_token(py_current_map_token, key, value);
    Py_RETURN_NONE;
}

static PyMethodDef aether_map_methods[] = {
    {"aether_map_get", py_aether_map_get, METH_VARARGS, "Get value from Aether shared map"},
    {"aether_map_put", py_aether_map_put, METH_VARARGS, "Put value to Aether shared map"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef aether_map_module = {
    PyModuleDef_HEAD_INIT, "_aether_map", NULL, -1, aether_map_methods
};

static int map_module_registered = 0;

static void register_map_module(void) {
    if (map_module_registered) return;
    PyObject* mod = PyModule_Create(&aether_map_module);
    if (mod) {
        PyObject* sys_modules = PyImport_GetModuleDict();
        PyDict_SetItemString(sys_modules, "_aether_map", mod);
        Py_DECREF(mod);
    }
    map_module_registered = 1;
}

int python_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token) {
    if (!code) return -1;
    python_init();
    register_map_module();

    // Freeze inputs and set active token
    extern void aether_shared_map_freeze_inputs_by_token(uint64_t);
    aether_shared_map_freeze_inputs_by_token(map_token);
    py_current_map_token = map_token;

    // Inject convenience imports so user code just calls aether_map_get/put
    const char* preamble =
        "from _aether_map import aether_map_get, aether_map_put\n";

    // Push perms and install checker
    _aether_ctx_stack[_aether_ctx_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = host_python_checker;

    // Run preamble + user code
    PyRun_SimpleString(preamble);
    int result = PyRun_SimpleString(code);

    // Restore
    _aether_sandbox_checker = prev;
    _aether_ctx_depth--;
    py_current_map_token = 0;

    return result;
}

#else
// Stubs when Python is not available
#include <stdio.h>
int python_init(void) {
    fprintf(stderr, "error: std.host.python not available (compile with AETHER_HAS_PYTHON)\n");
    return -1;
}
void python_finalize(void) {}
int python_run(const char* code) { (void)code; return python_init(); }
int python_run_sandboxed(void* perms,
    const char* code) {
  (void)perms; (void)code;
  return python_init();
}
int python_run_sandboxed_with_map(void* perms, const char* code,
    uint64_t token) {
  (void)perms; (void)code; (void)token;
  return python_init();
}
#endif

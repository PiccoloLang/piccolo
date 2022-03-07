
#include "../embedding.h"
#include "../util/memory.h"
#include "picStdlib.h"
#include "../gc.h"
#include "../util/file.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <ShlObj_core.h>
#else
#include <dlfcn.h>
#endif
#include <string.h>

struct dll {

#ifdef _WIN32
    HMODULE library;
#else
    void* handle;
#endif

    piccolo_Value close, get;

};

static piccolo_Value dllCloseNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    struct piccolo_Obj* obj = PICCOLO_AS_OBJ(self);
    struct dll* dll = PICCOLO_GET_PAYLOAD(obj, struct dll);
#ifdef _WIN32
    FreeLibrary(dll->library);
#else
    dlclose(dll->handle);
#endif
    return PICCOLO_NIL_VAL();
}

static piccolo_Value dllGetNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value symbolNameVal = argv[0];
    if(!PICCOLO_IS_STRING(symbolNameVal)) {
        piccolo_runtimeError(engine, "Function name must be a string.");
        return PICCOLO_NIL_VAL();
    }
    struct piccolo_ObjString* symbolName = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(symbolNameVal);
    struct piccolo_Obj* obj = PICCOLO_AS_OBJ(self);
    struct dll* dll = PICCOLO_GET_PAYLOAD(obj, struct dll);
    piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self);
#ifdef _WIN32
    void* dllObj = GetProcAddress(dll->library, symbolName->string);
    if(dllObj == NULL) {
        piccolo_runtimeError(engine, "Invalid DLL function.");
        return PICCOLO_NIL_VAL();
    }
    native = dllObj;
#else
    void* dllObj = dlsym(dll->handle, symbolName->string);
    if(dllObj == NULL) {
        piccolo_runtimeError(engine, "Invalid DLL function.");
        return PICCOLO_NIL_VAL();
    }
    native = (piccolo_Value(*)(struct piccolo_Engine *, int, piccolo_Value *, piccolo_Value))dllObj;
#endif
    return PICCOLO_OBJ_VAL(piccolo_makeNative(engine, native));
}

static void gcMarkDll(void* payload) {
    struct dll* dll = (struct dll*)payload;
    piccolo_gcMarkValue(dll->close);
    piccolo_gcMarkValue(dll->get);
}

static piccolo_Value indexDll(void* payload, struct piccolo_Engine* engine, piccolo_Value key, bool set, piccolo_Value value) {
    struct dll* dll = (struct dll*)payload;
    if(!PICCOLO_IS_STRING(key)) {
        piccolo_runtimeError(engine, "Property must be a string.");
        return PICCOLO_NIL_VAL();
    }
    struct piccolo_ObjString* keyStr = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(key);
    if(strcmp(keyStr->string, "close") == 0) {
        if(set) {
            piccolo_runtimeError(engine, "Cannot set close.");
            return PICCOLO_NIL_VAL();
        }
        return dll->close;
    }
    if(strcmp(keyStr->string, "get") == 0) {
        if(set) {
            piccolo_runtimeError(engine, "Cannot set get.");
            return PICCOLO_NIL_VAL();
        }
        return dll->get;
    }
    piccolo_runtimeError(engine, "Invalid property.");
    return PICCOLO_NIL_VAL();
}

static piccolo_Value openNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value pathVal = argv[0];
    if(!PICCOLO_IS_STRING(pathVal)) {
        piccolo_runtimeError(engine, "Argument must be a string.");
        return PICCOLO_NIL_VAL();
    }
    char* path = ((struct piccolo_ObjString*)PICCOLO_AS_OBJ(pathVal))->string;
    struct piccolo_ObjNativeStruct* dllNativeStruct = (struct piccolo_ObjNativeStruct*)PICCOLO_ALLOCATE_NATIVE_STRUCT(engine, struct dll, "dll");
    dllNativeStruct->gcMark = gcMarkDll;
    dllNativeStruct->index = indexDll;
    struct dll* dll = PICCOLO_GET_PAYLOAD(dllNativeStruct, struct dll);

#ifdef _WIN32
    dll->library = LoadLibrary(path);
    if(dll->library == NULL) {
        size_t pathLen = strlen(path);
        char buf[4096];
        for(int i = 0; i < engine->searchPaths.count; i++) {
            piccolo_applyRelativePathToFilePath(buf, path, pathLen, engine->searchPaths.values[i]);
            dll->library = LoadLibrary(buf);
            if(dll->library != NULL)
                break;
        }
        if(dll->library == NULL) {
            piccolo_runtimeError(engine, "Could not open DLL.");
            return PICCOLO_NIL_VAL();
        }
    }
#else
    dll->handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
    if(dll->handle == NULL) {
        size_t pathLen = strlen(path);
        char buf[4096];
        for(int i = 0; i < engine->searchPaths.count; i++) {
            piccolo_applyRelativePathToFilePath(buf, path, pathLen, engine->searchPaths.values[i]);
            dll->handle = dlopen(buf, RTLD_LAZY | RTLD_GLOBAL);
            if(dll->handle != NULL)
                break;
        }
        if(dll->handle == NULL) {
            piccolo_runtimeError(engine, "Could not open DLL.");
            return PICCOLO_NIL_VAL();
        }
    }
#endif
    dll->close = PICCOLO_OBJ_VAL(piccolo_makeBoundNative(engine, dllCloseNative, PICCOLO_OBJ_VAL(dllNativeStruct)));
    dll->get = PICCOLO_OBJ_VAL(piccolo_makeBoundNative(engine, dllGetNative, PICCOLO_OBJ_VAL(dllNativeStruct)));
    return PICCOLO_OBJ_VAL(dllNativeStruct);
}

void piccolo_addDLLLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* dll = piccolo_createPackage(engine);
    dll->packageName = "dll";
    struct piccolo_Type* str = piccolo_simpleType(engine, PICCOLO_TYPE_STR);
    struct piccolo_Type* any = piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
    piccolo_defineGlobalWithType(engine, dll, "open", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, openNative)), piccolo_makeFnType(engine, any, 1, str));
    #ifdef __APPLE__
    const char* extension = "dylib";
    #endif
    #ifdef _WIN32
    const char* extension = "dll";
    #endif
    #ifdef __linux__
    const char* extension = "so";
    #endif
    piccolo_defineGlobalWithType(engine, dll, "extension", PICCOLO_OBJ_VAL(piccolo_copyString(engine, extension, strlen(extension))), piccolo_simpleType(engine, PICCOLO_TYPE_STR));
}

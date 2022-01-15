
#include "object.h"
#include "util/memory.h"
#include "engine.h"
#include "package.h"
#include "util/strutil.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>

uint32_t piccolo_hashHashmapKey(piccolo_Value key) {
    if(PICCOLO_IS_NUM(key)) {
        double val = PICCOLO_AS_NUM(key);
        int32_t i;
        val = frexp(val, &i) * -(double)INT_MIN;
        int32_t ni = (int32_t)val;
        uint32_t u = (uint32_t)i + (uint32_t)ni;
        return u <= INT_MAX ? u : ~u;
    }
    if(PICCOLO_IS_BOOL(key)) {
        return 1000000007 * PICCOLO_AS_BOOL(key);
    }
    if(PICCOLO_IS_OBJ(key)) {
        struct piccolo_Obj* keyObj = PICCOLO_AS_OBJ(key);
        if(keyObj->type == PICCOLO_OBJ_STRING) {
            return ((struct piccolo_ObjString*)keyObj)->hash;
        }
    }
    return 0;
}

bool piccolo_compareHashmapKeys(piccolo_Value a, piccolo_Value b) {
    return piccolo_valuesEqual(a, b);
}

bool piccolo_HashmapIsBaseKey(piccolo_Value key) {
    return PICCOLO_IS_NIL(key);
}

static inline struct piccolo_HashmapValue getBase() {
    struct piccolo_HashmapValue result;
    result.exists = false;
    return result;
}

PICCOLO_HASHMAP_IMPL(piccolo_Value, struct piccolo_HashmapValue, Hashmap, PICCOLO_NIL_VAL(), (getBase()))

bool piccolo_isObjOfType(piccolo_Value val, enum piccolo_ObjType type) {
    return PICCOLO_IS_OBJ(val) && PICCOLO_AS_OBJ(val)->type == type;
}

struct piccolo_Obj* allocateObj(struct piccolo_Engine* engine, enum piccolo_ObjType type, size_t size) {
    struct piccolo_Obj* obj = PICCOLO_REALLOCATE("obj", engine, NULL, 0, size);
    obj->next = engine->objs;
    engine->objs = obj;
    obj->type = type;
    obj->printed = false;
    return obj;
}

struct piccolo_ObjNativeStruct* piccolo_allocNativeStruct(struct piccolo_Engine* engine, size_t size, const char* typename) {
    struct piccolo_ObjNativeStruct* nativeStruct = (struct piccolo_ObjNativeStruct*) allocateObj(engine, PICCOLO_OBJ_NATIVE_STRUCT, sizeof(struct piccolo_ObjNativeStruct) + size);
    nativeStruct->payloadSize = size;
    nativeStruct->free = NULL;
    nativeStruct->gcMark = NULL;
    nativeStruct->index = NULL;
    nativeStruct->typename = typename;
    return nativeStruct;
}

void piccolo_freeObj(struct piccolo_Engine* engine, struct piccolo_Obj* obj) {
    size_t objSize = 10;
    switch(obj->type) {
        case PICCOLO_OBJ_STRING: {
            objSize = sizeof(struct piccolo_ObjString);
            PICCOLO_REALLOCATE("free string", engine, ((struct piccolo_ObjString*)obj)->string, ((struct piccolo_ObjString*)obj)->len + 1, 0);
            break;
        }
        case PICCOLO_OBJ_ARRAY: {
            objSize = sizeof(struct piccolo_ObjArray);
            piccolo_freeValueArray(engine, &((struct piccolo_ObjArray*)obj)->array);
            break;
        }
        case PICCOLO_OBJ_HASHMAP: {
            objSize = sizeof(struct piccolo_ObjHashmap);
            piccolo_freeHashmap(engine, &((struct piccolo_ObjHashmap*)obj)->hashmap);
            break;
        }
        case PICCOLO_OBJ_FUNC: {
            objSize = sizeof(struct piccolo_ObjFunction);
            struct piccolo_ObjFunction* func = (struct piccolo_ObjFunction*)obj;
            piccolo_freeBytecode(engine, &func->bytecode);
            break;
        }
        case PICCOLO_OBJ_UPVAL: {
            objSize = sizeof(struct piccolo_ObjUpval);
            if(!((struct piccolo_ObjUpval*)obj)->open)
                PICCOLO_REALLOCATE("free heap upval", engine, ((struct piccolo_ObjUpval*)obj)->val.ptr, sizeof(struct piccolo_Value), 0);
            break;
        }
        case PICCOLO_OBJ_CLOSURE: {
            objSize = sizeof(struct piccolo_ObjClosure);
            struct piccolo_ObjClosure* closure = (struct piccolo_ObjClosure*)obj;
            PICCOLO_REALLOCATE("free upval array", engine, closure->upvals, sizeof(struct piccolo_ObjUpval*) * closure->upvalCnt, 0);
            break;
        }
        case PICCOLO_OBJ_NATIVE_FN: {
            objSize = sizeof(struct piccolo_ObjNativeFn);
            break;
        }
        case PICCOLO_OBJ_NATIVE_STRUCT: {
            struct piccolo_ObjNativeStruct* nativeStruct = (struct piccolo_ObjNativeStruct*)obj;
            objSize = sizeof(struct piccolo_ObjNativeStruct) + nativeStruct->payloadSize;
            if(nativeStruct->free != NULL)
                nativeStruct->free(PICCOLO_GET_PAYLOAD(obj, void));
            break;
        }
        case PICCOLO_OBJ_PACKAGE: break;
    }
    PICCOLO_REALLOCATE("free obj", engine, obj, objSize, 0);
}

static uint32_t hashString(const char* string, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)string[i];
        hash *= 16777619;
    }
    return hash;
}

static struct piccolo_ObjString* newString(struct piccolo_Engine* engine, char* string, int len) {
    struct piccolo_ObjString* result = (struct piccolo_ObjString*) PICCOLO_ALLOCATE_OBJ(engine, struct piccolo_ObjString, PICCOLO_OBJ_STRING);
    result->string = string;
    result->len = len;
    result->utf8Len = 0;
    const char* curr = string;
    while(*curr != '\0') {
        result->utf8Len++;
        curr += piccolo_strutil_utf8Chars(*curr);
    }
    result->hash = hashString(string, len);
    return result;
}

struct piccolo_ObjString* piccolo_takeString(struct piccolo_Engine* engine, char* string) {
    return newString(engine, string, strlen(string));
}

struct piccolo_ObjString* piccolo_copyString(struct piccolo_Engine* engine, const char* string, int len) {
    char* copy = PICCOLO_REALLOCATE("string copy", engine, NULL, 0, len + 1);
    memcpy(copy, string, len);
    copy[len] = '\0';
    return newString(engine, copy, len);
}

struct piccolo_ObjArray* piccolo_newArray(struct piccolo_Engine* engine, int len) {
    struct piccolo_ObjArray* array = PICCOLO_ALLOCATE_OBJ(engine, struct piccolo_ObjArray, PICCOLO_OBJ_ARRAY);
    piccolo_initValueArray(&array->array);
    for(int i = 0; i < len; i++) {
        piccolo_writeValueArray(engine, &array->array, PICCOLO_NIL_VAL());
        if(!array->array.values) {
            piccolo_runtimeError(engine, "Failed to allocate for new array.");
            return NULL;
        }
    }
    return array;
}

struct piccolo_ObjHashmap* piccolo_newHashmap(struct piccolo_Engine* engine) {
    struct piccolo_ObjHashmap* hashmap = PICCOLO_ALLOCATE_OBJ(engine, struct piccolo_ObjHashmap, PICCOLO_OBJ_HASHMAP);
    piccolo_initHashmap(&hashmap->hashmap);
    return hashmap;
}

struct piccolo_ObjFunction* piccolo_newFunction(struct piccolo_Engine* engine) {
    struct piccolo_ObjFunction* function = PICCOLO_ALLOCATE_OBJ(engine, struct piccolo_ObjFunction, PICCOLO_OBJ_FUNC);
    piccolo_initBytecode(&function->bytecode);
    function->arity = 0;
    return function;
}

struct piccolo_ObjUpval* piccolo_newUpval(struct piccolo_Engine* engine, int idx) {
    struct piccolo_ObjUpval* upval = PICCOLO_ALLOCATE_OBJ(engine, struct piccolo_ObjUpval, PICCOLO_OBJ_UPVAL);
    upval->val.idx = idx;
    upval->open = true;
    upval->next = engine->openUpvals;
    engine->openUpvals = upval;
    return upval;
}

struct piccolo_ObjClosure* piccolo_newClosure(struct piccolo_Engine* engine, struct piccolo_ObjFunction* function, int upvals) {
    struct piccolo_ObjClosure* closure = PICCOLO_ALLOCATE_OBJ(engine, struct piccolo_ObjClosure, PICCOLO_OBJ_CLOSURE);
    closure->prototype = function;
    closure->upvals = PICCOLO_REALLOCATE("upval array", engine, NULL, 0, sizeof(struct piccolo_ObjUpval*) * upvals);
    closure->upvalCnt = upvals;
    return closure;
}

struct piccolo_ObjNativeFn* piccolo_makeNative(struct piccolo_Engine* engine, piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, piccolo_Value self)) {
    struct piccolo_ObjNativeFn* nativeFn = PICCOLO_ALLOCATE_OBJ(engine, struct piccolo_ObjNativeFn, PICCOLO_OBJ_NATIVE_FN);
    nativeFn->native = native;
    return nativeFn;
}

struct piccolo_ObjNativeFn* piccolo_makeBoundNative(struct piccolo_Engine* engine, piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, piccolo_Value self), piccolo_Value self) {
    struct piccolo_ObjNativeFn* nativeFn = piccolo_makeNative(engine, native);
    nativeFn->self = self;
    return nativeFn;
}

struct piccolo_Package* piccolo_newPackage(struct piccolo_Engine* engine) {
    struct piccolo_Package* package = PICCOLO_ALLOCATE_OBJ(engine, struct piccolo_Package, PICCOLO_OBJ_PACKAGE);
    return package;
}
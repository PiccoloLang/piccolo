
#include "object.h"
#include "util/memory.h"
#include "engine.h"
#include "package.h"
#include <string.h>

static struct piccolo_Obj* allocateObj(struct piccolo_Engine* engine, enum piccolo_ObjType type, size_t size) {
    struct piccolo_Obj* obj = PICCOLO_REALLOCATE("obj", engine, NULL, 0, size);
    obj->next = engine->objs;
    engine->objs = obj;
    obj->type = type;
    return obj;
}

#define ALLOCATE_OBJ(engine, type, objType) ((type*)allocateObj(engine, objType, sizeof(type)))

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
        case PICCOLO_OBJ_FUNC: {
            objSize = sizeof(struct piccolo_ObjFunction);
            struct piccolo_ObjFunction* func = (struct piccolo_ObjFunction*)obj;
            piccolo_freeBytecode(engine, &func->bytecode);
            break;
        }
        case PICCOLO_OBJ_UPVAL: {
            objSize = sizeof(struct piccolo_ObjUpval);
            if(!((struct piccolo_ObjUpval*)obj)->open)
                PICCOLO_REALLOCATE("free heap upval", engine, ((struct piccolo_ObjUpval*)obj)->valPtr, sizeof(struct piccolo_Value), 0);
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

static struct piccolo_ObjString* newString(struct piccolo_Engine* engine, const char* string, int len) {
    struct piccolo_ObjString* result = ALLOCATE_OBJ(engine, struct piccolo_ObjString, PICCOLO_OBJ_STRING);
    result->string = string;
    result->len = len;
    result->hash = hashString(string, len);
    return result;
}

struct piccolo_ObjString* piccolo_takeString(struct piccolo_Engine* engine, const char* string) {
    return newString(engine, string, strlen(string));
}

struct piccolo_ObjString* piccolo_copyString(struct piccolo_Engine* engine, const char* string, int len) {
    char* copy = PICCOLO_REALLOCATE("string copy", engine, NULL, 0, len + 1);
    memcpy(copy, string, len);
    copy[len] = '\0';
    return newString(engine, copy, len);
}

struct piccolo_ObjArray* piccolo_newArray(struct piccolo_Engine* engine, int len) {
    struct piccolo_ObjArray* array = ALLOCATE_OBJ(engine, struct piccolo_ObjArray, PICCOLO_OBJ_ARRAY);
    piccolo_initValueArray(&array->array);
    for(int i = 0; i < len; i++)
        piccolo_writeValueArray(engine, &array->array, PICCOLO_NIL_VAL());
    return array;
}

struct piccolo_ObjFunction* piccolo_newFunction(struct piccolo_Engine* engine) {
    struct piccolo_ObjFunction* function = ALLOCATE_OBJ(engine, struct piccolo_ObjFunction, PICCOLO_OBJ_FUNC);
    piccolo_initBytecode(&function->bytecode);
    function->arity = 0;
    return function;
}

struct piccolo_ObjUpval* piccolo_newUpval(struct piccolo_Engine* engine, piccolo_Value* ptr) {
    struct piccolo_ObjUpval* upval = ALLOCATE_OBJ(engine, struct piccolo_ObjUpval, PICCOLO_OBJ_UPVAL);
    upval->valPtr = ptr;
    upval->open = true;
    upval->next = engine->openUpvals;
    engine->openUpvals = upval;
    return upval;
}

struct piccolo_ObjClosure* piccolo_newClosure(struct piccolo_Engine* engine, struct piccolo_ObjFunction* function, int upvals) {
    struct piccolo_ObjClosure* closure = ALLOCATE_OBJ(engine, struct piccolo_ObjClosure, PICCOLO_OBJ_CLOSURE);
    closure->prototype = function;
    closure->upvals = PICCOLO_REALLOCATE("upval array", engine, NULL, 0, sizeof(struct piccolo_ObjUpval*) * upvals);
    closure->upvalCnt = upvals;
    return closure;
}

struct piccolo_ObjNativeFn* piccolo_makeNative(struct piccolo_Engine* engine, piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args)) {
    struct piccolo_ObjNativeFn* nativeFn = ALLOCATE_OBJ(engine, struct piccolo_ObjNativeFn, PICCOLO_OBJ_NATIVE_FN);
    nativeFn->native = native;
    return nativeFn;
}

struct piccolo_Package* piccolo_newPackage(struct piccolo_Engine* engine) {
    struct piccolo_Package* package = ALLOCATE_OBJ(engine, struct piccolo_Package, PICCOLO_OBJ_PACKAGE);
    return package;
}
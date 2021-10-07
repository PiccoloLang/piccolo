
#include "object.h"
#include "util/memory.h"
#include <string.h>

static struct piccolo_Obj* allocateObj(struct piccolo_Engine* engine, enum piccolo_ObjType type, size_t size) {
    struct piccolo_Obj* obj = reallocate(engine, NULL, 0, size);
    obj->type = type;
    return obj;
}

#define ALLOCATE_OBJ(engine, type, objType) ((type*)allocateObj(engine, objType, sizeof(type)))

static struct piccolo_ObjString* newString(struct piccolo_Engine* engine, const char* string, int len) {
    struct piccolo_ObjString* result = ALLOCATE_OBJ(engine, struct piccolo_ObjString, PICCOLO_OBJ_STRING);
    result->string = string;
    result->len = len;
    return result;
}

struct piccolo_ObjString* piccolo_takeString(struct piccolo_Engine* engine, const char* string) {
    return newString(engine, string, strlen(string));
}

struct piccolo_ObjString* piccolo_copyString(struct piccolo_Engine* engine, const char* string, int len) {
    char* copy = reallocate(engine, NULL, 0, len + 1);
    memcpy(copy, string, len);
    copy[len] = '\0';
    return newString(engine, copy, len);
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
    closure->upvals = reallocate(engine, NULL, 0, sizeof(struct piccolo_ObjUpval*) * upvals);
    return closure;
}

struct piccolo_ObjNativeFn* piccolo_makeNative(struct piccolo_Engine* engine, piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args)) {
    struct piccolo_ObjNativeFn* nativeFn = ALLOCATE_OBJ(engine, struct piccolo_ObjNativeFn, PICCOLO_OBJ_NATIVE_FN);
    nativeFn->obj.type = PICCOLO_OBJ_NATIVE_FN;
    nativeFn->native = native;
    return nativeFn;
}
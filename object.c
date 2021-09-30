
#include "object.h"
#include "util/memory.h"

static struct piccolo_Obj* allocateObj(struct piccolo_Engine* engine, enum piccolo_ObjType type, size_t size) {
    struct piccolo_Obj* obj = reallocate(engine, NULL, 0, size);
    obj->type = type;
    return obj;
}

#define ALLOCATE_OBJ(engine, type, objType) ((type*)allocateObj(engine, objType, sizeof(type)))

struct piccolo_ObjFunction* piccolo_newFunction(struct piccolo_Engine* engine) {
    struct piccolo_ObjFunction* function = ALLOCATE_OBJ(engine, struct piccolo_ObjFunction, PICCOLO_OBJ_FUNC);
    piccolo_initBytecode(&function->bytecode);
    function->arity = 0;
    return function;
}

struct piccolo_ObjNativeFn* piccolo_makeNative(struct piccolo_Engine* engine, piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args)) {
    struct piccolo_ObjNativeFn* nativeFn = ALLOCATE_OBJ(engine, struct piccolo_ObjNativeFn, PICCOLO_OBJ_NATIVE_FN);
    nativeFn->obj.type = PICCOLO_OBJ_NATIVE_FN;
    nativeFn->native = native;
    return nativeFn;
}
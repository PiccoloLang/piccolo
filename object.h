
#ifndef PICCOLO_OBJECT_H
#define PICCOLO_OBJECT_H

#include "bytecode.h"
#include "engine.h"

enum piccolo_ObjType {
    PICCOLO_OBJ_FUNC,
    PICCOLO_OBJ_NATIVE_FN
};

struct piccolo_Obj {
    enum piccolo_ObjType type;
};

struct piccolo_ObjFunction {
    struct piccolo_Obj obj;
    struct piccolo_Bytecode bytecode;
    int arity;
};

struct piccolo_ObjNativeFn {
    struct piccolo_Obj obj;
    piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args);
};

struct piccolo_ObjFunction* piccolo_newFunction(struct piccolo_Engine* engine);
struct piccolo_ObjNativeFn* piccolo_makeNative(struct piccolo_Engine* engine, piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args));

#endif

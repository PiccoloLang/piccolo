
#ifndef PICCOLO_OBJECT_H
#define PICCOLO_OBJECT_H

#include "bytecode.h"
#include "engine.h"

enum piccolo_ObjType {
    PICCOLO_OBJ_FUNC
};

struct piccolo_Obj {
    enum piccolo_ObjType type;
};

struct piccolo_ObjFunction {
    struct piccolo_Obj obj;
    struct piccolo_Bytecode bytecode;
    int arity;
};

struct piccolo_ObjFunction* piccolo_newFunction(struct piccolo_Engine* engine);

#endif

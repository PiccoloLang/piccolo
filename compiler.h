
#ifndef PICCOLO_COMPILER_H
#define PICCOLO_COMPILER_H

#include <stdbool.h>

#include "scanner.h"
#include "util/dynarray.h"

struct piccolo_Engine;
struct piccolo_Package;

struct piccolo_Variable {
    int slot;
    const char* nameStart;
    size_t nameLen;
    bool mutable;
};

struct piccolo_Upvalue {
    int slot;
    bool local;
    bool mutable;
};

PICCOLO_DYNARRAY_HEADER(struct piccolo_Variable, Variable)
PICCOLO_DYNARRAY_HEADER(struct piccolo_Upvalue, Upvalue)

struct piccolo_Compiler {
    struct piccolo_Package* package;
    struct piccolo_VariableArray* globals;
    struct piccolo_VariableArray locals;
    struct piccolo_UpvalueArray upvals;
    struct piccolo_Compiler* enclosing;
    bool hadError;
};

bool piccolo_compilePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);

#endif

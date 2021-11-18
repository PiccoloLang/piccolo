
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

PICCOLO_DYNARRAY_HEADER(struct piccolo_Variable, Variable)

struct piccolo_Compiler {
    struct piccolo_Package* package;
    struct piccolo_VariableArray* globals;
    struct piccolo_VariableArray locals;
    bool hadError;
};

bool piccolo_compilePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);

#endif

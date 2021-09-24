
#ifndef PICCOLO_COMPILER_H
#define PICCOLO_COMPILER_H

#include <stdbool.h>

#include "scanner.h"
#include "util/dynarray.h"

struct piccolo_Engine;
struct piccolo_Package;

struct piccolo_Variable {
    int slot;
    const char* name;
    size_t nameLen;
};

PICCOLO_DYNARRAY_HEADER(struct piccolo_Variable, Variable)

struct piccolo_Compiler {
    struct piccolo_Scanner scanner;
    struct piccolo_Token current;
    struct piccolo_VariableArray* globals;
    struct piccolo_VariableArray locals;
    bool hadError;
    bool cycled;
};

bool piccolo_compilePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);

#endif

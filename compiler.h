
#ifndef PICCOLO_COMPILER_H
#define PICCOLO_COMPILER_H

#include <stdbool.h>

#include "scanner.h"

struct piccolo_Engine;
struct piccolo_Package;

struct piccolo_Compiler {
    struct piccolo_Scanner scanner;
    struct piccolo_Token current;
    bool hadError;
};

bool piccolo_compilePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);

#endif

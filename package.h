
#ifndef PICCOLO_PACKAGE_H
#define PICCOLO_PACKAGE_H

#include "bytecode.h"
#include "compiler.h"

struct piccolo_Package {
    struct piccolo_Bytecode bytecode;
    char* source;
    struct piccolo_ValueArray globals;
    struct piccolo_VariableArray globalVars;
};

struct piccolo_Engine;

struct piccolo_Package* piccolo_loadPackage(struct piccolo_Engine* engine, const char* filepath);
void piccolo_freePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);

#endif

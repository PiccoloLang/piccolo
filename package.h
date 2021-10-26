
#ifndef PICCOLO_PACKAGE_H
#define PICCOLO_PACKAGE_H

#include "compiler.h"
#include "util/dynarray.h"
#include "object.h"
#include "bytecode.h"

struct piccolo_Package {
    struct piccolo_Obj obj;
    struct piccolo_Bytecode bytecode;
    const char* source;
    struct piccolo_ValueArray globals;
    struct piccolo_VariableArray globalVars;
    const char* packageName;
};

struct piccolo_Package* piccolo_createPackage(struct piccolo_Engine* engine);
struct piccolo_Package* piccolo_loadPackage(struct piccolo_Engine* engine, const char* filepath);
void piccolo_freePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);

#endif

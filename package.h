
#ifndef PICCOLO_PACKAGE_H
#define PICCOLO_PACKAGE_H

#include "compiler.h"
#include "util/dynarray.h"
#include "util/hashmap.h"
#include "object.h"
#include "bytecode.h"

PICCOLO_HASHMAP_HEADER(struct piccolo_ObjString*, int, GlobalTable)

struct piccolo_Package {
    struct piccolo_Obj obj;
    struct piccolo_Bytecode bytecode;
    char* source;
    struct piccolo_ValueArray globals;
    struct piccolo_GlobalTable globalIdxs;
    const char* packageName;
    bool compiled, executed, compilationError;
};

struct piccolo_Package* piccolo_createPackage(struct piccolo_Engine* engine);
struct piccolo_Package* piccolo_loadPackage(struct piccolo_Engine* engine, const char* filepath);
void piccolo_freePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);

#endif

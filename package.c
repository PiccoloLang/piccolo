
#include "package.h"

#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "compiler.h"
#include "util/file.h"
#include "object.h"
#include "util/memory.h"

uint32_t piccolo_hashGlobalTableKey(struct piccolo_ObjString* key) {
    return key->hash;
}

bool piccolo_compareGlobalTableKeys(struct piccolo_ObjString* a, struct piccolo_ObjString* b) {
    return strcmp(a->string, b->string) == 0;
}

PICCOLO_HASHMAP_IMPL(struct piccolo_ObjString*, int, GlobalTable, NULL, -1)

static void initPackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    piccolo_initValueArray(&package->globals);
    piccolo_initGlobalTable(&package->globalIdxs);
    piccolo_initBytecode(&package->bytecode);
    package->compiled = false;
    package->executed = false;
    package->source = NULL;
}

struct piccolo_Package* piccolo_createPackage(struct piccolo_Engine* engine) {
    struct piccolo_Package* package = piccolo_newPackage(engine);
    piccolo_writePackageArray(engine, &engine->packages, package);
    initPackage(engine, package);
    return package;
}

struct piccolo_Package* piccolo_loadPackage(struct piccolo_Engine* engine, const char* filepath) {
    struct piccolo_Package* package = piccolo_createPackage(engine);

    package->packageName = filepath;

    package->source = piccolo_readFile(filepath);
    if(package->source == NULL) {
        piccolo_enginePrintError(engine, "Could not load package %s\n", filepath);
        return package;
    }

    if(!piccolo_compilePackage(engine, package)) {
        piccolo_enginePrintError(engine, "Compilation error.\n");
        return package;
    }

    return package;
}

void piccolo_freePackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    piccolo_freeValueArray(engine, &package->globals);
    piccolo_freeGlobalTable(engine, &package->globalIdxs);
    piccolo_freeBytecode(engine, &package->bytecode);
    if(package->source != NULL)
        free(package->source);
}
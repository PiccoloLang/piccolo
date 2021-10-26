
#include "package.h"

#include <stdlib.h>

#include "engine.h"
#include "compiler.h"
#include "util/file.h"
#include "object.h"
#include "time.h"
#include "util/memory.h"

static void initPackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    piccolo_initValueArray(&package->globals);
    piccolo_initVariableArray(&package->globalVars);
    piccolo_initBytecode(&package->bytecode);
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
    initPackage(engine, package);

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

    if(!piccolo_executePackage(engine, package)) {
        piccolo_enginePrintError(engine, "Runtime error.\n");
        return package;
    }
    return package;
}

void piccolo_freePackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    piccolo_freeValueArray(engine, &package->globals);
    for(int i = 0; i < package->globalVars.count; i++)
        if(!package->globalVars.values[i].nameInSource)
            PICCOLO_REALLOCATE("var name free", engine, package->globalVars.values[i].name, package->globalVars.values[i].nameLen + 1, 0);
    piccolo_freeVariableArray(engine, &package->globalVars);
    piccolo_freeBytecode(engine, &package->bytecode);
    if(package->source != NULL)
        free(package->source);
}
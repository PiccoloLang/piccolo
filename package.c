
#include "package.h"

#include <stdlib.h>

#include "engine.h"
#include "compiler.h"
#include "util/file.h"
#include "object.h"


// TODO: decouple this
#include "embedding.h"
#include <stdio.h>
static struct piccolo_Value printNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    for(int i = 0; i < argc; i++) {
        piccolo_printValue(args[i]);
        printf(" ");
    }
    printf("\n");
    return NIL_VAL();
}

static void initPackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    piccolo_initValueArray(&package->globals);
    piccolo_initVariableArray(&package->globalVars);
    piccolo_initBytecode(&package->bytecode);
}

struct piccolo_Package* piccolo_loadPackage(struct piccolo_Engine* engine, const char* filepath) {
    struct piccolo_Package* package = &engine->package;
    initPackage(engine, package);

    package->source = piccolo_readFile(filepath);
    if(package->source == NULL) {
        piccolo_enginePrintError(engine, "Could not load package %s\n", filepath);
        return package;
    }

    piccolo_defineGlobal(engine, package, "print", OBJ_VAL(piccolo_makeNative(engine, printNative)));

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
    piccolo_freeVariableArray(engine, &package->globalVars);
    piccolo_freeBytecode(engine, &package->bytecode);
    free(package->source);
}
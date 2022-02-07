
#include "embedding.h"
#include "typecheck.h"

#include <string.h>

void piccolo_addSearchPath(struct piccolo_Engine* engine, const char* path) {
    if(path[strlen(path) - 1] != '/') {
        // TODO: This should be fixed in the CLI once the package path changes are merged
        piccolo_enginePrintError(engine, "Incorrectly formatted package path '%s'\n", path);
        return;
    }
    piccolo_writeStringArray(engine, &engine->searchPaths, path);
}

void piccolo_defineGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen, struct piccolo_Value value) {
    struct piccolo_ObjString* varName = piccolo_copyString(engine, name, nameLen);
    piccolo_setGlobalTable(engine, &package->globalIdxs, varName, package->globals.count);
    piccolo_writeValueArray(engine, &package->globals, value);
    piccolo_writeTypeArray(engine, &package->types, piccolo_simpleType(engine, PICCOLO_TYPE_ANY));
}

void piccolo_defineGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, struct piccolo_Value value) {
    piccolo_defineGlobalWithNameSize(engine, package, name, strlen(name), value);
}

piccolo_Value piccolo_getGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen) {
    struct piccolo_ObjString* varName = piccolo_copyString(engine, name, nameLen);
    int idx = piccolo_getGlobalTable(engine, &package->globalIdxs, varName);
    if(idx == -1) {
        return PICCOLO_NIL_VAL(); // TODO: There needs to be some way to dilliniate actual nil from errors
    }
    return package->globals.values[idx];
}

piccolo_Value piccolo_getGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name) {
    return piccolo_getGlobalWithNameSize(engine, package, name, strlen(name));
}

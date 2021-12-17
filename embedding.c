
#include "embedding.h"

#include <string.h>

void piccolo_addSearchPath(struct piccolo_Engine* engine, const char* path) {
    piccolo_writeStringArray(engine, &engine->searchPaths, path);
}

void piccolo_defineGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen, struct piccolo_Value value) {
    struct piccolo_ObjString* varName = piccolo_copyString(engine, name, nameLen);
    piccolo_setGlobalTable(engine, &package->globalIdxs, varName, package->globals.count);
    piccolo_writeValueArray(engine, &package->globals, value);
}

void piccolo_defineGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, struct piccolo_Value value) {
    piccolo_defineGlobalWithNameSize(engine, package, name, strlen(name), value);
}
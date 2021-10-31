
#include "embedding.h"

#include <string.h>

void piccolo_defineGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen, struct piccolo_Value value) {
//    struct piccolo_Variable global;
//    global.name = PICCOLO_REALLOCATE("global varname", engine, NULL, 0, nameLen + 1);
//    global.name[nameLen] = '\0';
//    strcpy(global.name, name);
//    global.nameLen = nameLen;
//    global.slot = package->globals.count;
//    global.nameInSource = false;
//    piccolo_writeVariableArray(engine, &package->globalVars, global);
    struct piccolo_ObjString* varName = piccolo_copyString(engine, name, nameLen);
    piccolo_setGlobalTable(engine, &package->globalIdxs, varName, package->globals.count);
    piccolo_writeValueArray(engine, &package->globals, value);
}

void piccolo_defineGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, struct piccolo_Value value) {
    piccolo_defineGlobalWithNameSize(engine, package, name, strlen(name), value);
}
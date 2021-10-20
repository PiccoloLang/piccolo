
#include "embedding.h"

#include <string.h>

piccolo_Value piccolo_getGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen) {
    for(int i = 0; i < package->globalVars.count; i++) {
        if(package->globalVars.values[i].nameLen == nameLen && memcmp(package->globalVars.values[i].name, name, nameLen) == 0)
            return package->globals.values[package->globalVars.values[i].slot];
    }
    return PICCOLO_NIL_VAL();
}

piccolo_Value piccolo_getGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name) {
    return piccolo_getGlobalWithNameSize(engine, package, name, strlen(name));
}

void piccolo_defineGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen, struct piccolo_Value value) {
    struct piccolo_Variable global;
    global.name = PICCOLO_REALLOCATE("global varname", engine, NULL, 0, nameLen + 1);
    global.name[nameLen] = '\0';
    strcpy(global.name, name);
    global.nameLen = nameLen;
    global.slot = package->globals.count;
    global.nameInSource = false;
    piccolo_writeVariableArray(engine, &package->globalVars, global);
    piccolo_writeValueArray(engine, &package->globals, value);
}

void piccolo_defineGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, struct piccolo_Value value) {
    piccolo_defineGlobalWithNameSize(engine, package, name, strlen(name), value);
}
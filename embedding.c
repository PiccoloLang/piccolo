
#include "embedding.h"

#include <string.h>

piccolo_Value piccolo_getGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen) {
    for(int i = 0; i < package->globalVars.count; i++) {
        if(package->globalVars.values[i].nameLen == nameLen && memcmp(package->globalVars.values[i].name, name, nameLen) == 0)
            return package->globals.values[package->globalVars.values[i].slot];
    }
    return NIL_VAL();
}


piccolo_Value piccolo_getGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name) {
    return piccolo_getGlobalWithNameSize(engine, package, name, strlen(name));
}



#include "../embedding.h"
#include "../util/memory.h"
#include "picStdlib.h"
#include <stdlib.h>
#include <stdio.h>

static piccolo_Value shellNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value cmdVal = argv[0];
    if(!PICCOLO_IS_OBJ(cmdVal) || PICCOLO_AS_OBJ(cmdVal)->type != PICCOLO_OBJ_STRING) {
        piccolo_runtimeError(engine, "Command must be a string.");
        return PICCOLO_NIL_VAL();
    }
    struct piccolo_ObjString* cmd = PICCOLO_AS_OBJ(cmdVal);
    system(cmd->string);
    return PICCOLO_NIL_VAL();
}

void piccolo_addOSLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* os = piccolo_createPackage(engine);
    os->packageName = "os";
    piccolo_defineGlobal(engine, os, "shell", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, shellNative)));
}
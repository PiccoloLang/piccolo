
#include "../embedding.h"
#include "../util/memory.h"
#include "picStdlib.h"
#include <stdlib.h>
#include <stdio.h>

static piccolo_Value randomValNative(struct piccolo_Engine* engine, int argc, piccolo_Value argv) {
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    double val = (double)rand() / RAND_MAX;
    return PICCOLO_NUM_VAL(val);
}

void piccolo_addRandomLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* random = piccolo_createPackage(engine);
    random->packageName = "random";
    piccolo_defineGlobal(engine, random, "val", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, randomValNative)));
}

#include "stdlib.h"
#include "../embedding.h"

#include <stdio.h>

static piccolo_Value printNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    for(int i = 0; i < argc; i++) {
        piccolo_printValue(args[i]);
        printf(" ");
    }
    printf("\n");
    return PICCOLO_NIL_VAL();
}

void piccolo_addIOLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* io = piccolo_createPackage(engine);
    io->packageName = "io";
    piccolo_defineGlobal(engine, io, "print", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, printNative)));
}
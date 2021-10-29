
#include "stdlib.h"
#include "../embedding.h"

#include <stdio.h>
#include <time.h>

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

static piccolo_Value clockNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    double time = (double)clock() / (double)CLOCKS_PER_SEC;
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    }
    return PICCOLO_NUM_VAL(time);
}

static struct piccolo_Value sleepNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        if(!PICCOLO_IS_NUM(args[0])) {
            piccolo_runtimeError(engine, "Sleep time must be a number.");
        } else {
            double time = PICCOLO_AS_NUM(args[0]);
            clock_t startTime = clock();
            while(clock() - startTime < time * CLOCKS_PER_SEC) {}
        }
    }
    return PICCOLO_NIL_VAL();
}

void piccolo_addTimeLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* time = piccolo_createPackage(engine);
    time->packageName = "time";
    piccolo_defineGlobal(engine, time, "clock", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, clockNative)));
    piccolo_defineGlobal(engine, time, "sleep", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, sleepNative)));
}

#include "picStdlib.h"
#include "../embedding.h"
#include "../util/memory.h"
#include <time.h>

static piccolo_Value clockNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, piccolo_Value self) {
    double time = (double)clock() / (double)CLOCKS_PER_SEC;
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    }
    return PICCOLO_NUM_VAL(time);
}

static piccolo_Value sleepNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, piccolo_Value self) {
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
    struct piccolo_Type* num = piccolo_simpleType(engine, PICCOLO_TYPE_NUM);
    struct piccolo_Type* nil = piccolo_simpleType(engine, PICCOLO_TYPE_NIL);
    piccolo_defineGlobalWithType(engine, time, "clock", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, clockNative)), piccolo_makeFnType(engine, num, 0));
    piccolo_defineGlobalWithType(engine, time, "sleep", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, sleepNative)), piccolo_makeFnType(engine, nil, 1, num));
}

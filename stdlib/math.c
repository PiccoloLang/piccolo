
#include "picStdlib.h"
#include "../embedding.h"
#include "../util/memory.h"
#include <stdio.h>

static piccolo_Value minNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv) {
    if(argc != 2) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value aVal = argv[0];
    piccolo_Value bVal = argv[1];
    if(!PICCOLO_IS_NUM(aVal) || !PICCOLO_IS_NUM(bVal)) {
        piccolo_runtimeError(engine, "Both arguments must be numbers.");
        return PICCOLO_NIL_VAL();
    }

    double a = PICCOLO_AS_NUM(aVal);
    double b = PICCOLO_AS_NUM(bVal);
    return PICCOLO_NUM_VAL(a < b ? a : b);
}

static piccolo_Value maxNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv) {
    if(argc != 2) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value aVal = argv[0];
    piccolo_Value bVal = argv[1];
    if(!PICCOLO_IS_NUM(aVal) || !PICCOLO_IS_NUM(bVal)) {
        piccolo_runtimeError(engine, "Both arguments must be numbers.");
        return PICCOLO_NIL_VAL();
    }

    double a = PICCOLO_AS_NUM(aVal);
    double b = PICCOLO_AS_NUM(bVal);
    return PICCOLO_NUM_VAL(a > b ? a : b);
}

static piccolo_Value mapNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv) {
    if(argc != 5) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    } else {
        for(int i = 0; i < 5; i++) {
            if(!PICCOLO_IS_NUM(argv[i])) {
                piccolo_runtimeError(engine, "All arguments must be numbers.");
                return PICCOLO_NIL_VAL();
            }
        }
        double value = PICCOLO_AS_NUM(argv[0]);
        double fromL = PICCOLO_AS_NUM(argv[1]);
        double fromR = PICCOLO_AS_NUM(argv[2]);
        double toL = PICCOLO_AS_NUM(argv[3]);
        double toR = PICCOLO_AS_NUM(argv[4]);
        double result = ((value - fromL) / (fromR - fromL)) * (toR - toL) + toL;
        return PICCOLO_NUM_VAL(result);
    }
}

#include <math.h>

static piccolo_Value sinNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        if(!PICCOLO_IS_NUM(argv[0])) {
            piccolo_runtimeError(engine, "Angle must be a number.");
        } else {
            double angle = PICCOLO_AS_NUM(argv[0]);
            double result = sin(angle);
            return PICCOLO_NUM_VAL(result);
        }
    }
    return PICCOLO_NIL_VAL();
}

static piccolo_Value cosNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        if(!PICCOLO_IS_NUM(argv[0])) {
            piccolo_runtimeError(engine, "Angle must be a number.");
        } else {
            double angle = PICCOLO_AS_NUM(argv[0]);
            double result = cos(angle);
            return PICCOLO_NUM_VAL(result);
        }
    }
    return PICCOLO_NIL_VAL();
}

static piccolo_Value tanNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        if(!PICCOLO_IS_NUM(argv[0])) {
            piccolo_runtimeError(engine, "Angle must be a number.");
        } else {
            double angle = PICCOLO_AS_NUM(argv[0]);
            double result = tan(angle);
            return PICCOLO_NUM_VAL(result);
        }
    }
    return PICCOLO_NIL_VAL();
}

static piccolo_Value floorNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv) { 
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        if(!PICCOLO_IS_NUM(argv[0])) {
            piccolo_runtimeError(engine, "Value must be a number.");
        } else {
            double val = PICCOLO_AS_NUM(argv[0]);
            int64_t valFloored = val;
            return PICCOLO_NUM_VAL(valFloored);
        }
    }
    return PICCOLO_NIL_VAL();
}

void piccolo_addMathLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* math = piccolo_createPackage(engine);
    math->packageName = "math";
    piccolo_defineGlobal(engine, math, "min", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, minNative)));
    piccolo_defineGlobal(engine, math, "max", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, maxNative)));
    piccolo_defineGlobal(engine, math, "map", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, mapNative)));
    piccolo_defineGlobal(engine, math, "pi", PICCOLO_NUM_VAL(3.14159265359));
    piccolo_defineGlobal(engine, math, "sin", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, sinNative)));
    piccolo_defineGlobal(engine, math, "cos", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, cosNative)));
    piccolo_defineGlobal(engine, math, "tan", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, tanNative)));
    piccolo_defineGlobal(engine, math, "floor", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, floorNative)));
}

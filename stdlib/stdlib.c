
#include "stdlib.h"
#include "../embedding.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static piccolo_Value printNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    for(int i = 0; i < argc; i++) {
        piccolo_printValue(args[i]);
        printf(" ");
    }
    printf("\n");
    return PICCOLO_NIL_VAL();
}

static piccolo_Value inputNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }

    size_t lenMax = 64;
    size_t len = 0;
    char* line = PICCOLO_REALLOCATE("input string", engine, NULL, 0, lenMax);
    char* curr = line;
    for(;;) {
        if(line == NULL) {
            piccolo_runtimeError(engine, "Could not read input.");
            return PICCOLO_NIL_VAL();
        }
        int c = fgetc(stdin);
        if(c == EOF || c == '\n') {
            break;
        }
        *curr = c;
        curr++;
        len++;
        if(len >= lenMax) {
            lenMax *= 2;
            line = PICCOLO_REALLOCATE("input string", engine, line, lenMax / 2, lenMax);
        }
    }
    *curr = '\0';
    return PICCOLO_OBJ_VAL(piccolo_takeString(engine, line));
}

void piccolo_addIOLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* io = piccolo_createPackage(engine);
    io->packageName = "io";
    piccolo_defineGlobal(engine, io, "print", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, printNative)));
    piccolo_defineGlobal(engine, io, "input", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, inputNative)));
}

static piccolo_Value clockNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    double time = (double)clock() / (double)CLOCKS_PER_SEC;
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    }
    return PICCOLO_NUM_VAL(time);
}

static piccolo_Value sleepNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
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
            piccolo_runtimeError(engine, "Angle must be number.");
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
            piccolo_runtimeError(engine, "Angle must be number.");
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
            piccolo_runtimeError(engine, "Angle must be number.");
        } else {
            double angle = PICCOLO_AS_NUM(argv[0]);
            double result = tan(angle);
            return PICCOLO_NUM_VAL(result);
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
}

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

#include "../debug/disassembler.h"
static piccolo_Value disassembleFunctionNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        piccolo_Value val = args[0];
        if(!PICCOLO_IS_OBJ(val) || PICCOLO_AS_OBJ(val)->type != PICCOLO_OBJ_CLOSURE) {
            piccolo_runtimeError(engine, "Cannot dissasemble %s.", piccolo_getTypeName(val));
        } else {
            struct piccolo_ObjClosure* function = (struct piccolo_ObjClosure*) PICCOLO_AS_OBJ(val);
            piccolo_disassembleBytecode(&function->prototype->bytecode);
        }
    }
    return PICCOLO_NIL_VAL();
}

static int assertions = 0;
static int assertionsMet = 0;

static piccolo_Value assertNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        piccolo_Value val = args[0];
        if(!PICCOLO_IS_BOOL(val)) {
            piccolo_runtimeError(engine, "Expected assertion to be a boolean.");
        } else {
            assertions++;
            bool assertion = PICCOLO_AS_BOOL(val);
            if(assertion) {
                printf("\x1b[32m[OK]\x1b[0m ASSERTION MET\n");
                assertionsMet++;
            } else {
                printf("\x1b[31m[ERROR]\x1b[0m ASSERTION FAILED\n");
            }
        }
    }
    return PICCOLO_NIL_VAL();
}

static piccolo_Value printAssertionResultsNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args) {
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
    } else {
        if(assertionsMet == assertions) {
            printf("\x1b[32m%d / %d ASSERTIONS MET! ALL OK\x1b[0m\n", assertionsMet, assertions);
        } else {
            printf("\x1b[31m%d / %d ASSERTIONS MET.\x1b[0m\n", assertionsMet, assertions);
        }
    }
    return PICCOLO_NIL_VAL();
}

void piccolo_addDebugLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* debug = piccolo_createPackage(engine);
    debug->packageName = "debug";
    piccolo_defineGlobal(engine, debug, "disassemble", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, disassembleFunctionNative)));
    piccolo_defineGlobal(engine, debug, "assert", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, assertNative)));
    piccolo_defineGlobal(engine, debug, "printAssertionResults", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, printAssertionResultsNative)));
}

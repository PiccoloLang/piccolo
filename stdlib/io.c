
#include "picStdlib.h"
#include "../embedding.h"
#include "../util/memory.h"
#include <stdio.h>

static piccolo_Value printNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, piccolo_Value self) {
    for(int i = 0; i < argc; i++) {
        piccolo_printValue(args[i]);
        printf(" ");
    }
    printf("\n");
    return PICCOLO_NIL_VAL();
}

static piccolo_Value inputNative(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, piccolo_Value self) {
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }

    size_t lenMax = 64;
    size_t len = 0;
    char* line = (char*)PICCOLO_REALLOCATE("input string", engine, NULL, 0, lenMax);
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
            line = (char*)PICCOLO_REALLOCATE("input string", engine, line, lenMax / 2, lenMax);
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

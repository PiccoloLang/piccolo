
#include "engine.h"
#include <stdbool.h>
#include <stdio.h>

void piccolo_initEngine(struct piccolo_Engine* engine, void (*printError)(const char* format, ...)) {
    piccolo_initValueArray(&engine->stack);

    engine->printError = printError;
}

void piccolo_freeEngine(struct piccolo_Engine* engine) {
    piccolo_freePackage(engine, &engine->package);
}

static void run(struct piccolo_Engine* engine) {
#define READ_BYTE() (*(engine->ip++))
#define READ_PARAM() ((READ_BYTE() << 8) + READ_BYTE())
    while(true) {
        uint8_t opcode = READ_BYTE();
        switch(opcode) {
            case OP_RETURN:
                return;
            case OP_CONST: {
                piccolo_enginePushStack(engine, engine->bytecode->constants.values[READ_PARAM()]);
                break;
            }
            case OP_ADD: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                piccolo_enginePushStack(engine, b + a);
                break;
            }
            case OP_SUB: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                piccolo_enginePushStack(engine, b - a);
                break;
            }
            case OP_MUL: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                piccolo_enginePushStack(engine, b * a);
                break;
            }
            case OP_DIV: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                piccolo_enginePushStack(engine, b / a);
                break;
            }
            default: {
                break;
            }
        }
    }
#undef READ_BYTE
}

void piccolo_executeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode) {
    engine->bytecode = bytecode;
    engine->ip = bytecode->code.values;
    run(engine);
    printValue(piccolo_enginePopStack(engine));
    printf("\n");
}


void piccolo_enginePushStack(struct piccolo_Engine* engine, piccolo_Value value) {
    piccolo_writeValueArray(engine, &engine->stack, value);
}

piccolo_Value piccolo_enginePopStack(struct piccolo_Engine* engine) {
    return piccolo_popValueArray(&engine->stack);
}

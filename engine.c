
#include "engine.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "util/strutil.h"

void piccolo_initEngine(struct piccolo_Engine* engine, void (*printError)(const char* format, va_list)) {
    piccolo_initValueArray(&engine->stack);

    engine->printError = printError;
}

void piccolo_freeEngine(struct piccolo_Engine* engine) {
    piccolo_freePackage(engine, &engine->package);
}

static bool run(struct piccolo_Engine* engine) {
#define READ_BYTE() (*(engine->ip++))
#define READ_PARAM() ((READ_BYTE() << 8) + READ_BYTE())
    while(true) {
        engine->prevIp = engine->ip;
        uint8_t opcode = READ_BYTE();
        switch(opcode) {
            case OP_RETURN:
                return true;
            case OP_CONST: {
                piccolo_enginePushStack(engine, engine->bytecode->constants.values[READ_PARAM()]);
                break;
            }
            case OP_ADD: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                if(!IS_NUM(a) || !IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot add %s and %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                    break;
                }
                piccolo_enginePushStack(engine, NUM_VAL(AS_NUM(b) + AS_NUM(a)));
                break;
            }
            case OP_SUB: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                if(!IS_NUM(a) || !IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot subtract %s from %s.", piccolo_getTypeName(a), piccolo_getTypeName(b));
                    break;
                }
                piccolo_enginePushStack(engine, NUM_VAL(AS_NUM(b) - AS_NUM(a)));
                break;
            }
            case OP_MUL: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                if(!IS_NUM(a) || !IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot multiply %s by %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                    break;
                }
                piccolo_enginePushStack(engine, NUM_VAL(AS_NUM(b) * AS_NUM(a)));
                break;
            }
            case OP_DIV: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                if(!IS_NUM(a) || !IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot divide %s by %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                    break;
                }
                piccolo_enginePushStack(engine, NUM_VAL(AS_NUM(b) / AS_NUM(a)));
                break;
            }
            default: {
                break;
            }
        }

        if(engine->hadError) {
            return false;
        }
    }
#undef READ_BYTE
}

bool piccolo_executePackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    engine->currentPackage = package;
    return piccolo_executeBytecode(engine, &package->bytecode);
}

bool piccolo_executeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode) {
    engine->bytecode = bytecode;
    engine->ip = bytecode->code.values;
    bool result = run(engine);

    piccolo_printValue(piccolo_enginePopStack(engine));
    printf("\n");

    return result;
}

void piccolo_enginePrintError(struct piccolo_Engine* engine, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
}

void piccolo_enginePushStack(struct piccolo_Engine* engine, piccolo_Value value) {
    piccolo_writeValueArray(engine, &engine->stack, value);
}

piccolo_Value piccolo_enginePopStack(struct piccolo_Engine* engine) {
    return piccolo_popValueArray(&engine->stack);
}

void piccolo_runtimeError(struct piccolo_Engine* engine, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
    piccolo_enginePrintError(engine, "\n");

    int charIdx = engine->bytecode->charIdxs.values[engine->prevIp - engine->bytecode->code.values];
    struct piccolo_strutil_LineInfo opLine = piccolo_strutil_getLine(engine->currentPackage->source, charIdx);
    piccolo_enginePrintError(engine, "[line %d] %.*s\n", opLine.line + 1, opLine.lineEnd - opLine.lineStart, opLine.lineStart);

    int lineNumberDigits = 0;
    int lineNumber = opLine.line + 1;
    while(lineNumber > 0) {
        lineNumberDigits++;
        lineNumber /= 10;
    }
    piccolo_enginePrintError(engine, "%* ^", 9 + lineNumberDigits + engine->currentPackage->source + charIdx - opLine.lineStart);
    piccolo_enginePrintError(engine, "\n");

    engine->hadError = true;
}
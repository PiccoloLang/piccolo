
#include "engine.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "util/strutil.h"
#include "object.h"

void piccolo_initEngine(struct piccolo_Engine* engine, void (*printError)(const char* format, va_list)) {
    engine->printError = printError;
    engine->stackTop = engine->stack;
}

void piccolo_freeEngine(struct piccolo_Engine* engine) {
    piccolo_freePackage(engine, &engine->package);
}

static void evaporatePointer(piccolo_Value* value) {
    while(IS_PTR((*value)))
        *value = *AS_PTR((*value));
}

static bool run(struct piccolo_Engine* engine) {
#define READ_BYTE() (engine->frames[engine->currFrame].bytecode->code.values[engine->frames[engine->currFrame].ip++])
#define READ_PARAM() ((READ_BYTE() << 8) + READ_BYTE())
    engine->hadError = false;
    while(true) {
        engine->frames[engine->currFrame].prevIp = engine->frames[engine->currFrame].ip;
        uint8_t opcode = READ_BYTE();
        switch(opcode) {
            case OP_RETURN:
                if(engine->currFrame == 0)
                    return true;
                engine->currFrame--;
                engine->frames[engine->currFrame].prevIp = engine->frames[engine->currFrame].ip;
                engine->frames[engine->currFrame].bytecode = engine->frames[engine->currFrame].bytecode;
                break;
            case OP_CONST: {
                piccolo_enginePushStack(engine, engine->frames[engine->currFrame].bytecode->constants.values[READ_PARAM()]);
                break;
            }
            case OP_ADD: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                evaporatePointer(&a);
                evaporatePointer(&b);
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
                evaporatePointer(&a);
                evaporatePointer(&b);
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
                evaporatePointer(&a);
                evaporatePointer(&b);
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
                evaporatePointer(&a);
                evaporatePointer(&b);
                if(!IS_NUM(a) || !IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot divide %s by %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                    break;
                }
                piccolo_enginePushStack(engine, NUM_VAL(AS_NUM(b) / AS_NUM(a)));
                break;
            }
            case OP_EQUAL: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                evaporatePointer(&a);
                piccolo_Value b = piccolo_enginePopStack(engine);
                evaporatePointer(&b);
                if(IS_NUM(a) && IS_NUM(b)) {
                    piccolo_enginePushStack(engine, BOOL_VAL(AS_NUM(a) == AS_NUM(b)));
                    break;
                }
                piccolo_enginePushStack(engine, BOOL_VAL(false));
            }
            case OP_POP_STACK: {
                piccolo_enginePopStack(engine);
                break;
            }
            case OP_GET_STACK: {
                int slot = READ_PARAM();
                piccolo_enginePushStack(engine, PTR_VAL(engine->frames[engine->currFrame].varStack + slot));
                break;
            }
            case OP_GET_GLOBAL: {
                int slot = READ_PARAM();
                while(engine->currentPackage->globals.count <= slot)
                    piccolo_writeValueArray(engine, &engine->currentPackage->globals, NIL_VAL());
                piccolo_enginePushStack(engine, PTR_VAL(engine->currentPackage->globals.values + slot));
                break;
            }
            case OP_SET: {
                piccolo_Value value = piccolo_enginePopStack(engine);
                evaporatePointer(&value);
                piccolo_Value ptr = piccolo_enginePopStack(engine);
                if(!IS_PTR(ptr)) {
                    piccolo_runtimeError(engine, "Cannot assign to %s", piccolo_getTypeName(ptr));
                    break;
                }
                *AS_PTR(ptr) = value;
                piccolo_enginePushStack(engine, value);
                break;
            }
            case OP_JUMP: {
                int jumpDist = READ_PARAM();
                engine->frames[engine->currFrame].ip += jumpDist - 3;
                break;
            }
            case OP_JUMP_FALSE: {
                int jumpDist = READ_PARAM();
                piccolo_Value condition = piccolo_enginePopStack(engine);
                if(!IS_BOOL(condition)) {
                    piccolo_runtimeError(engine, "Condition must be a boolean.");
                    break;
                }
                if(!AS_BOOL(condition)) {
                    engine->frames[engine->currFrame].ip += jumpDist - 3;
                }
                break;
            }
            case OP_CALL: {
                int argCount = READ_PARAM();
                engine->currFrame++;
                for(int i = argCount - 1; i >= 0; i--) {
                    engine->frames[engine->currFrame].varStack[i] = piccolo_enginePopStack(engine);
                    evaporatePointer(&engine->frames[engine->currFrame].varStack[i]);
                }
                piccolo_Value func = piccolo_enginePopStack(engine);
                evaporatePointer(&func);

                if(!IS_OBJ(func) || (AS_OBJ(func)->type != PICCOLO_OBJ_FUNC && AS_OBJ(func)->type != PICCOLO_OBJ_NATIVE_FN)) {
                    engine->currFrame--;
                    piccolo_runtimeError(engine, "Cannot call %s.", piccolo_getTypeName(func));
                    break;
                }
                enum piccolo_ObjType type = AS_OBJ(func)->type;

                if(type == PICCOLO_OBJ_FUNC) {
                    struct piccolo_ObjFunction *funcObj = (struct piccolo_ObjFunction*)AS_OBJ(func);
                    engine->frames[engine->currFrame].ip = engine->frames[engine->currFrame].prevIp = 0;
                    engine->frames[engine->currFrame].bytecode = &funcObj->bytecode;
                    if (funcObj->arity != argCount) {
                        piccolo_runtimeError(engine, "Wrong argument count.");
                        break;
                    }
                }
                if(type == PICCOLO_OBJ_NATIVE_FN) {
                    struct piccolo_ObjNativeFn* native = (struct piccolo_ObjNativeFn*)AS_OBJ(func);
                    piccolo_enginePushStack(engine, native->native(engine, argCount, engine->frames[engine->currFrame].varStack));
                    engine->currFrame--;
                    break;
                }
                break;
            }
            default: {
                piccolo_runtimeError(engine, "Unknown opcode.");
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
    engine->currFrame = 0;
    engine->frames[engine->currFrame].ip = 0;
    engine->frames[engine->currFrame].bytecode = bytecode;
    engine->stackTop = engine->stack;
    return run(engine);
}

void piccolo_enginePrintError(struct piccolo_Engine* engine, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
}

void piccolo_enginePushStack(struct piccolo_Engine* engine, piccolo_Value value) {
    *engine->stackTop = value;
    engine->stackTop++;
}

piccolo_Value piccolo_enginePopStack(struct piccolo_Engine* engine) {
    piccolo_Value value = piccolo_enginePeekStack(engine);
    engine->stackTop--;
    return value;
}

piccolo_Value piccolo_enginePeekStack(struct piccolo_Engine* engine) {
    return *(engine->stackTop - 1);
}

void piccolo_runtimeError(struct piccolo_Engine* engine, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
    piccolo_enginePrintError(engine, "\n");

    int charIdx = engine->frames[engine->currFrame].bytecode->charIdxs.values[engine->frames[engine->currFrame].prevIp];
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
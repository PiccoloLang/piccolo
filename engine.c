
#include "engine.h"

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "util/strutil.h"
#include "object.h"
#include "gc.h"

void piccolo_initEngine(struct piccolo_Engine* engine, void (*printError)(const char* format, va_list)) {
    engine->printError = printError;
    engine->stackTop = engine->stack;
    engine->openUpvals = NULL;
    engine->liveMemory = 0;
    engine->objs = NULL;
#ifdef PICCOLO_ENABLE_MEMORY_TRACKER
    engine->track = NULL;
#endif
}

void piccolo_freeEngine(struct piccolo_Engine* engine) {
    piccolo_freePackage(engine, &engine->package);
    struct piccolo_Obj* curr = engine->objs;
    while(curr != NULL) {
        struct piccolo_Obj* toFree = curr;
        curr = curr->next;
        piccolo_freeObj(engine, toFree);
    }
}

static void evaporatePointer(piccolo_Value* value) {
    while(PICCOLO_IS_PTR((*value)))
        *value = *PICCOLO_AS_PTR((*value));
}

static bool run(struct piccolo_Engine* engine) {
#define READ_BYTE() (engine->frames[engine->currFrame].bytecode->code.values[engine->frames[engine->currFrame].ip++])
#define READ_PARAM() ((READ_BYTE() << 8) + READ_BYTE())
    engine->hadError = false;
    while(true) {
        engine->frames[engine->currFrame].prevIp = engine->frames[engine->currFrame].ip;
        uint8_t opcode = READ_BYTE();
        switch(opcode) {
            case PICCOLO_OP_RETURN:
                if(engine->currFrame == 0)
                    return true;
                engine->currFrame--;
                engine->frames[engine->currFrame].prevIp = engine->frames[engine->currFrame].ip;
                evaporatePointer(engine->stackTop - 1);
                break;
            case PICCOLO_OP_CONST: {
                piccolo_enginePushStack(engine, engine->frames[engine->currFrame].bytecode->constants.values[READ_PARAM()]);
                break;
            }
            case PICCOLO_OP_ADD: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                evaporatePointer(&a);
                evaporatePointer(&b);
                if(PICCOLO_IS_NUM(a) && PICCOLO_IS_NUM(b)) {
                    piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(PICCOLO_AS_NUM(b) + PICCOLO_AS_NUM(a)));
                    break;
                }
                if(PICCOLO_IS_OBJ(a) && PICCOLO_IS_OBJ(b) && PICCOLO_AS_OBJ(a)->type == PICCOLO_OBJ_STRING && PICCOLO_AS_OBJ(b)->type == PICCOLO_OBJ_STRING) {
                    struct piccolo_ObjString* bStr = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(a);
                    struct piccolo_ObjString* aStr = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(b);
                    char* result = PICCOLO_REALLOCATE("string concat", engine, NULL, 0, aStr->len + bStr->len + 1);
                    memcpy(result, aStr->string, aStr->len);
                    memcpy(result + aStr->len, bStr->string, bStr->len);
                    result[aStr->len + bStr->len] = '\0';
                    struct piccolo_ObjString* resultStr = piccolo_takeString(engine, result);
                    piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(resultStr));
                    break;
                }
                piccolo_runtimeError(engine, "Cannot add %s and %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                break;
            }
            case PICCOLO_OP_SUB: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                evaporatePointer(&a);
                evaporatePointer(&b);
                if(!PICCOLO_IS_NUM(a) || !PICCOLO_IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot subtract %s from %s.", piccolo_getTypeName(a), piccolo_getTypeName(b));
                    break;
                }
                piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(PICCOLO_AS_NUM(b) - PICCOLO_AS_NUM(a)));
                break;
            }
            case PICCOLO_OP_MUL: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                evaporatePointer(&a);
                evaporatePointer(&b);
                if(PICCOLO_IS_NUM(a) && PICCOLO_IS_NUM(b)) {
                    piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(PICCOLO_AS_NUM(b) * PICCOLO_AS_NUM(a)));
                    break;
                }
                if((PICCOLO_IS_NUM(a) && PICCOLO_IS_OBJ(b) && PICCOLO_AS_OBJ(b)->type == PICCOLO_OBJ_STRING) ||
                   (PICCOLO_IS_NUM(b) && PICCOLO_IS_OBJ(a) && PICCOLO_AS_OBJ(a)->type == PICCOLO_OBJ_STRING)) {
                    int repetitions;
                    struct piccolo_ObjString* string;
                    if(PICCOLO_IS_NUM(a)) {
                        repetitions = PICCOLO_AS_NUM(a);
                        string = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(b);
                    } else {
                        repetitions = PICCOLO_AS_NUM(b);
                        string = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(a);
                    }
                    char* result = PICCOLO_REALLOCATE("string multiplication", engine, NULL, 0, repetitions * string->len + 1);
                    for(int i = 0; i < repetitions; i++)
                        memcpy(result + i * string->len, string->string, string->len);
                    result[repetitions * string->len] = '\0';
                    struct piccolo_ObjString* resultStr = piccolo_takeString(engine, result);
                    piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(resultStr));
                    break;
                }
                piccolo_runtimeError(engine, "Cannot multiply %s by %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                break;
            }
            case PICCOLO_OP_DIV: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                evaporatePointer(&a);
                evaporatePointer(&b);
                if(!PICCOLO_IS_NUM(a) || !PICCOLO_IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot divide %s by %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                    break;
                }
                piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(PICCOLO_AS_NUM(b) / PICCOLO_AS_NUM(a)));
                break;
            }
            case PICCOLO_OP_EQUAL: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                evaporatePointer(&a);
                piccolo_Value b = piccolo_enginePopStack(engine);
                evaporatePointer(&b);
                if(PICCOLO_IS_NUM(a) && PICCOLO_IS_NUM(b)) {
                    piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(PICCOLO_AS_NUM(a) == PICCOLO_AS_NUM(b)));
                    break;
                }
                if(PICCOLO_IS_BOOL(a) && PICCOLO_IS_BOOL(b)) {
                    piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(PICCOLO_AS_BOOL(a) == PICCOLO_AS_BOOL(b)));
                    break;
                }
                if(PICCOLO_IS_NIL(a) && PICCOLO_IS_NIL(b)) {
                    piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(true));
                    break;
                }
                if(PICCOLO_IS_OBJ(a) && PICCOLO_IS_OBJ(b)) {
                    struct piccolo_Obj* aObj = PICCOLO_AS_OBJ(a);
                    struct piccolo_Obj* bObj = PICCOLO_AS_OBJ(b);
                    if(aObj->type == PICCOLO_OBJ_STRING && bObj->type == PICCOLO_OBJ_STRING) {
                        struct piccolo_ObjString* aStr = (struct piccolo_ObjString*)aObj;
                        struct piccolo_ObjString* bStr = (struct piccolo_ObjString*)bObj;
                        if(aStr->len == bStr->len && strncmp(aStr->string, bStr->string, aStr->len) == 0) {
                            piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(true));
                            break;
                        }
                    }
                }
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(false));
                break;
            }
            case PICCOLO_OP_GREATER: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                evaporatePointer(&a);
                piccolo_Value b = piccolo_enginePopStack(engine);
                evaporatePointer(&b);
                if(!PICCOLO_IS_NUM(a) || !PICCOLO_IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot compare %s and %s.", piccolo_getTypeName(a), piccolo_getTypeName(b));
                    break;
                }
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(PICCOLO_AS_NUM(a) < PICCOLO_AS_NUM(b)));
                break;
            }
            case PICCOLO_OP_NOT: {
                piccolo_Value val = piccolo_enginePopStack(engine);
                evaporatePointer(&val);
                if(!PICCOLO_IS_BOOL(val)) {
                    piccolo_runtimeError(engine, "Cannot negate %s.", piccolo_getTypeName(val));
                    break;
                }
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(!PICCOLO_AS_BOOL(val)));
                break;
            }
            case PICCOLO_OP_LESS: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                evaporatePointer(&a);
                piccolo_Value b = piccolo_enginePopStack(engine);
                evaporatePointer(&b);
                if(!PICCOLO_IS_NUM(a) || !PICCOLO_IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot compare %s and %s.", piccolo_getTypeName(a), piccolo_getTypeName(b));
                }
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(PICCOLO_AS_NUM(a) > PICCOLO_AS_NUM(b)));
                break;
            }
            case PICCOLO_OP_CREATE_ARRAY: {
                int len = READ_PARAM();
                struct piccolo_ObjArray* array = piccolo_newArray(engine, len);
                for(int i = len - 1; i >= 0; i--)
                    array->array.values[i] = piccolo_enginePopStack(engine);
                piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(array));
                break;
            }
            case PICCOLO_OP_GET_IDX: {
                piccolo_Value idx = piccolo_enginePopStack(engine);
                evaporatePointer(&idx);
                piccolo_Value container = piccolo_enginePopStack(engine);
                evaporatePointer(&container);
                if(!PICCOLO_IS_OBJ(container)) {
                    piccolo_runtimeError(engine, "Cannot index %s", piccolo_getTypeName(container));
                } else {
                    struct piccolo_Obj* containerObj = PICCOLO_AS_OBJ(container);
                    switch(containerObj->type) {
                        case PICCOLO_OBJ_ARRAY: {
                            struct piccolo_ObjArray* array = (struct piccolo_ObjArray*)containerObj;
                            if(!PICCOLO_IS_NUM(idx)) {
                                piccolo_runtimeError(engine, "Cannot index array with %s.", piccolo_getTypeName(idx));
                                break;
                            }
                            int idxNum = PICCOLO_AS_NUM(idx);
                            if(idxNum < 0 || idxNum >= array->array.count) {
                                piccolo_runtimeError(engine, "Array index out of bounds.");
                                break;
                            }
                            piccolo_enginePushStack(engine, PICCOLO_PTR_VAL(array->array.values + idxNum));
                            break;
                        }
                        default:
                            piccolo_runtimeError(engine, "Cannot index %s", piccolo_getTypeName(container));
                    }
                }
                break;
            }
            case PICCOLO_OP_POP_STACK: {
                piccolo_enginePopStack(engine);
                break;
            }
            case PICCOLO_OP_GET_STACK: {
                int slot = READ_PARAM();
                piccolo_enginePushStack(engine, PICCOLO_PTR_VAL(engine->frames[engine->currFrame].varStack + slot));
                break;
            }
            case PICCOLO_OP_GET_GLOBAL: {
                int slot = READ_PARAM();
                while(engine->currentPackage->globals.count <= slot)
                    piccolo_writeValueArray(engine, &engine->currentPackage->globals, PICCOLO_NIL_VAL());
                piccolo_enginePushStack(engine, PICCOLO_PTR_VAL(engine->currentPackage->globals.values + slot));
                break;
            }
            case PICCOLO_OP_SET: {
                piccolo_Value value = piccolo_enginePopStack(engine);
                evaporatePointer(&value);
                piccolo_Value ptr = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_PTR(ptr)) {
                    piccolo_runtimeError(engine, "Cannot assign to %s", piccolo_getTypeName(ptr));
                    break;
                }
                *PICCOLO_AS_PTR(ptr) = value;
                piccolo_enginePushStack(engine, value);
                break;
            }
            case PICCOLO_OP_JUMP: {
                int jumpDist = READ_PARAM();
                engine->frames[engine->currFrame].ip += jumpDist - 3;
                break;
            }
            case PICCOLO_OP_JUMP_FALSE: {
                int jumpDist = READ_PARAM();
                piccolo_Value condition = piccolo_enginePopStack(engine);
                evaporatePointer(&condition);
                if(!PICCOLO_IS_BOOL(condition)) {
                    piccolo_runtimeError(engine, "Condition must be a boolean.");
                    break;
                }
                if(!PICCOLO_AS_BOOL(condition)) {
                    engine->frames[engine->currFrame].ip += jumpDist - 3;
                }
                break;
            }
            case PICCOLO_OP_REV_JUMP: {
                int jumpDist = READ_PARAM();
                engine->frames[engine->currFrame].ip -= jumpDist + 3;
                break;
            }
            case PICCOLO_OP_CALL: {
                int argCount = READ_PARAM();
                engine->currFrame++;
                for(int i = argCount - 1; i >= 0; i--) {
                    engine->frames[engine->currFrame].varStack[i] = piccolo_enginePopStack(engine);
                    evaporatePointer(&engine->frames[engine->currFrame].varStack[i]);
                }
                piccolo_Value func = piccolo_enginePopStack(engine);
                evaporatePointer(&func);

                if(engine->currFrame == 255) {
                    engine->currFrame--;
                    piccolo_runtimeError(engine, "Recursion stack overflow.");
                    break;
                }

                if(!PICCOLO_IS_OBJ(func) || (PICCOLO_AS_OBJ(func)->type != PICCOLO_OBJ_CLOSURE && PICCOLO_AS_OBJ(func)->type != PICCOLO_OBJ_NATIVE_FN)) {
                    engine->currFrame--;
                    piccolo_runtimeError(engine, "Cannot call %s.", piccolo_getTypeName(func));
                    break;
                }
                enum piccolo_ObjType type = PICCOLO_AS_OBJ(func)->type;

                if(type == PICCOLO_OBJ_CLOSURE) {
                    struct piccolo_ObjClosure* closureObj = (struct piccolo_ObjClosure*)PICCOLO_AS_OBJ(func);
                    struct piccolo_ObjFunction* funcObj = closureObj->prototype;
                    engine->frames[engine->currFrame].ip = engine->frames[engine->currFrame].prevIp = 0;
                    engine->frames[engine->currFrame].bytecode = &funcObj->bytecode;
                    engine->frames[engine->currFrame].closure = closureObj;
                    if (funcObj->arity != argCount) {
                        engine->currFrame--;
                        piccolo_runtimeError(engine, "Wrong argument count.");
                        break;
                    }
                }
                if(type == PICCOLO_OBJ_NATIVE_FN) {
                    struct piccolo_ObjNativeFn* native = (struct piccolo_ObjNativeFn*)PICCOLO_AS_OBJ(func);
                    engine->currFrame--;
                    piccolo_enginePushStack(engine, native->native(engine, argCount, engine->frames[engine->currFrame + 1].varStack));
                    break;
                }
                break;
            }
            case PICCOLO_OP_CLOSURE: {
                piccolo_Value val = piccolo_enginePopStack(engine);
                struct piccolo_ObjFunction* func = (struct piccolo_ObjFunction*)PICCOLO_AS_OBJ(val);
                int upvals = READ_PARAM();
                struct piccolo_ObjClosure* closure = piccolo_newClosure(engine, func, upvals);
                for(int i = 0; i < upvals; i++) {
                    int slot = READ_PARAM();
                    if(READ_BYTE())
                        closure->upvals[i] = piccolo_newUpval(engine, engine->frames[engine->currFrame].varStack + slot);
                    else
                        closure->upvals[i] = engine->frames[engine->currFrame].closure->upvals[slot];
                }
                piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(closure));
                break;
            }
            case PICCOLO_OP_GET_UPVAL: {
                int slot = READ_PARAM();
                piccolo_enginePushStack(engine, PICCOLO_PTR_VAL(engine->frames[engine->currFrame].closure->upvals[slot]->valPtr));
                break;
            }
            case PICCOLO_OP_CLOSE_UPVALS: {
                while(engine->openUpvals != NULL) {
                    struct piccolo_ObjUpval* upval = engine->openUpvals;
                    piccolo_Value* allocatedValue = PICCOLO_REALLOCATE("heap upval", engine, NULL, 0, sizeof(piccolo_Value));
                    *allocatedValue = *upval->valPtr;
                    upval->valPtr = allocatedValue;
                    upval->open = false;
                    engine->openUpvals = upval->next;
                }
                break;
            }
            default: {
                piccolo_runtimeError(engine, "Unknown opcode.");
                break;
            }
        }

        piccolo_collectGarbage(engine);

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

#ifdef PICCOLO_ENABLE_MEMORY_TRACKER

void piccolo_freeMemTracks(struct piccolo_Engine* engine) {
    struct piccolo_MemoryTrack* track = engine->track;
    while(track != NULL) {
        struct piccolo_MemoryTrack* toFree = track;
        track = track->next;
        free(toFree);
    }
}

#endif
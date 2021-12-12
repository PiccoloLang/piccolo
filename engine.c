
#include "engine.h"

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "util/strutil.h"
#include "object.h"
#include "gc.h"

// #define PICCOLO_ENABLE_ENGINE_DEBUG

#ifdef PICCOLO_ENABLE_ENGINE_DEBUG
#include <stdio.h>
#include "debug/disassembler.h"
#endif

PICCOLO_DYNARRAY_IMPL(struct piccolo_Package*, Package)

void piccolo_initEngine(struct piccolo_Engine* engine, void (*printError)(const char* format, va_list)) {
    piccolo_initValueArray(&engine->locals);
    engine->printError = printError;
    engine->stackTop = engine->stack;
    engine->openUpvals = NULL;
    engine->liveMemory = 0;
    engine->gcThreshold = 1024 * 64;
    engine->objs = NULL;
    engine->currFrame = -1;
    piccolo_initPackageArray(&engine->packages);
    for(int i = 0; i < 256; i++)
        engine->frames[i].closure = NULL;
#ifdef PICCOLO_ENABLE_MEMORY_TRACKER
    engine->track = NULL;
#endif
}

void piccolo_freeEngine(struct piccolo_Engine* engine) {
    for(int i = 0; i < engine->packages.count; i++)
        piccolo_freePackage(engine, engine->packages.values[i]);
    piccolo_freePackageArray(engine, &engine->packages);
    struct piccolo_Obj* curr = engine->objs;
    while(curr != NULL) {
        struct piccolo_Obj* toFree = curr;
        curr = curr->next;
        piccolo_freeObj(engine, toFree);
    }
}

#include <stdio.h>
static piccolo_Value indexing(struct piccolo_Engine* engine, struct piccolo_Obj* container, piccolo_Value idx, bool set, piccolo_Value value) {
    switch(container->type) {
        case PICCOLO_OBJ_STRING: {
            struct piccolo_ObjString* string = (struct piccolo_ObjString*)container;
            if(set) {
                piccolo_runtimeError(engine, "Strings are immutable.");
                return PICCOLO_NIL_VAL();
            }
            if(!PICCOLO_IS_NUM(idx)) {
                piccolo_runtimeError(engine, "Cannot index string with %s.", piccolo_getTypeName(idx));
                return PICCOLO_NIL_VAL();
            }
            int idxNum = PICCOLO_AS_NUM(idx);
            if(idxNum < 0 || idxNum >= string->len) {
                piccolo_runtimeError(engine, "Index %d out of bounds.", idxNum);
                return PICCOLO_NIL_VAL();
            }
            char str[2];
            str[0] = string->string[idxNum]; // TODO: Add unicode support
            str[1] = '\0';
            struct piccolo_ObjString* result = piccolo_copyString(engine, str, 1);
            return  PICCOLO_OBJ_VAL(result);
        }
        case PICCOLO_OBJ_ARRAY: {
            struct piccolo_ObjArray* array = (struct piccolo_ObjArray*)container;
            if(!PICCOLO_IS_NUM(idx)) {
                piccolo_runtimeError(engine, "Cannot index array with %s.", piccolo_getTypeName(idx));
                return PICCOLO_NIL_VAL();
            }
            int idxNum = PICCOLO_AS_NUM(idx);
            if(idxNum < 0 || idxNum >= array->array.count) {
                piccolo_runtimeError(engine, "Array index out of bounds.");
                return PICCOLO_NIL_VAL();
            }

            if(set) {
                array->array.values[idxNum] = value;
                return value;
            } else {
                return array->array.values[idxNum];
            }
            break;
        }
        case PICCOLO_OBJ_HASHMAP: {
            struct piccolo_ObjHashmap* hashmap = (struct piccolo_ObjHashmap*)container;
            if(set) {
                struct piccolo_HashmapValue val;
                val.exists = true;
                val.value = value;
                piccolo_setHashmap(engine, &hashmap->hashmap, idx, val);
                return value;
            } else {
                struct piccolo_HashmapValue result = piccolo_getHashmap(engine, &hashmap->hashmap, idx);
                if(result.exists) {
                    return result.value;
                } else {
                    piccolo_runtimeError(engine, "Key does not exist on map.");
                }
            }
            break;
        }
        case PICCOLO_OBJ_PACKAGE: {
            struct piccolo_Package* package = (struct piccolo_Package*)container;
            if(!PICCOLO_IS_OBJ(idx) || PICCOLO_AS_OBJ(idx)->type != PICCOLO_OBJ_STRING) {
                piccolo_runtimeError(engine, "Global variable name must be a string.");
                return PICCOLO_NIL_VAL();
                break;
            }
            struct piccolo_ObjString* varname = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(idx);
            int globalIdx = piccolo_getGlobalTable(engine, &package->globalIdxs, varname);
            if(globalIdx == -1) {
                piccolo_runtimeError(engine, "Global variable '%.*s' does not exists in package %s", varname->len, varname->string, package->packageName);
                return PICCOLO_NIL_VAL();
            } else {
                if(set) {
                    if(globalIdx & PICCOLO_GLOBAL_SLOT_MUTABLE_BIT) {
                        package->globals.values[globalIdx & (~PICCOLO_GLOBAL_SLOT_MUTABLE_BIT)] = value;
                        return value;
                    } else {
                        piccolo_runtimeError(engine, "Cannot set immutable variable '%.*s'", varname->len, varname->string);
                        return PICCOLO_NIL_VAL();
                    }
                } else {
                    return package->globals.values[globalIdx & (~PICCOLO_GLOBAL_SLOT_MUTABLE_BIT)];
                }
            }
            break;
        }
        default: {
            piccolo_runtimeError(engine, "Cannot index %s", piccolo_getTypeName(PICCOLO_OBJ_VAL(container)));
            return PICCOLO_NIL_VAL();
        }
    }
}

static bool shouldCloseUpval(struct piccolo_Engine* engine, struct piccolo_ObjUpval* upval) {
    return upval->valPtr >= engine->locals.values + engine->locals.count;
}

static bool run(struct piccolo_Engine* engine) {
#define READ_BYTE() (engine->frames[engine->currFrame].bytecode->code.values[engine->frames[engine->currFrame].ip++])
#define READ_PARAM() ((READ_BYTE() << 8) + READ_BYTE())
    engine->hadError = false;
    while(true) {
        engine->frames[engine->currFrame].prevIp = engine->frames[engine->currFrame].ip;
        enum piccolo_OpCode opcode = READ_BYTE();
        switch(opcode) {
            case PICCOLO_OP_RETURN:
                engine->currFrame--;
                if(engine->currFrame == -1)
                    return true;
                engine->frames[engine->currFrame].prevIp = engine->frames[engine->currFrame].ip;
                break;
            case PICCOLO_OP_CONST: {
                piccolo_enginePushStack(engine, engine->frames[engine->currFrame].bytecode->constants.values[READ_PARAM()]);
                break;
            }
            case PICCOLO_OP_ADD: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                if(PICCOLO_IS_NUM(a) && PICCOLO_IS_NUM(b)) {
                    piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(PICCOLO_AS_NUM(b) + PICCOLO_AS_NUM(a)));
                    break;
                }
                if(PICCOLO_IS_OBJ(a) && PICCOLO_IS_OBJ(b)) {
                    if(PICCOLO_AS_OBJ(a)->type == PICCOLO_OBJ_STRING && PICCOLO_AS_OBJ(b)->type == PICCOLO_OBJ_STRING) {
                        struct piccolo_ObjString* bStr = (struct piccolo_ObjString*) PICCOLO_AS_OBJ(a);
                        struct piccolo_ObjString* aStr = (struct piccolo_ObjString*) PICCOLO_AS_OBJ(b);
                        char *result = PICCOLO_REALLOCATE("string concat", engine, NULL, 0, aStr->len + bStr->len + 1);
                        memcpy(result, aStr->string, aStr->len);
                        memcpy(result + aStr->len, bStr->string, bStr->len);
                        result[aStr->len + bStr->len] = '\0';
                        struct piccolo_ObjString* resultStr = piccolo_takeString(engine, result);
                        piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(resultStr));
                        break;
                    }
                    if(PICCOLO_AS_OBJ(a)->type == PICCOLO_OBJ_ARRAY && PICCOLO_AS_OBJ(b)->type == PICCOLO_OBJ_ARRAY) {
                        struct piccolo_ObjArray* bArr = (struct piccolo_ObjArray*) PICCOLO_AS_OBJ(a);
                        struct piccolo_ObjArray* aArr = (struct piccolo_ObjArray*) PICCOLO_AS_OBJ(b);
                        struct piccolo_ObjArray* resultArr = piccolo_newArray(engine, aArr->array.count + bArr->array.count);
                        for(int i = 0; i < aArr->array.count; i++)
                            resultArr->array.values[i] = aArr->array.values[i];
                        for(int i = 0; i < bArr->array.count; i++)
                            resultArr->array.values[aArr->array.count + i] = bArr->array.values[i];
                        piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(resultArr));
                        break;
                    }
                }
                piccolo_runtimeError(engine, "Cannot add %s and %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                break;
            }
            case PICCOLO_OP_SUB: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
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
                if((PICCOLO_IS_NUM(a) && PICCOLO_IS_OBJ(b) && PICCOLO_AS_OBJ(b)->type == PICCOLO_OBJ_ARRAY) ||
                   (PICCOLO_IS_NUM(b) && PICCOLO_IS_OBJ(a) && PICCOLO_AS_OBJ(a)->type == PICCOLO_OBJ_ARRAY)) {
                    int repetitions;
                    struct piccolo_ObjArray* array;
                    if(PICCOLO_IS_NUM(a)) {
                        repetitions = PICCOLO_AS_NUM(a);
                        array = (struct piccolo_ObjArray*)PICCOLO_AS_OBJ(b);
                    } else {
                        repetitions = PICCOLO_AS_NUM(b);
                        array = (struct piccolo_ObjArray*)PICCOLO_AS_OBJ(a);
                    }
                    struct piccolo_ObjArray* result = piccolo_newArray(engine, array->array.count * repetitions);
                    for(int i = 0; i < repetitions; i++) {
                        for(int j = 0; j < array->array.count; j++) {
                            result->array.values[i * array->array.count + j] = array->array.values[j];
                        }
                    }
                    piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(result));
                    break;
                }
                piccolo_runtimeError(engine, "Cannot multiply %s by %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                break;
            }
            case PICCOLO_OP_DIV: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_NUM(a) || !PICCOLO_IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot divide %s by %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                    break;
                }
                piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(PICCOLO_AS_NUM(b) / PICCOLO_AS_NUM(a)));
                break;
            }
            case PICCOLO_OP_MOD: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_NUM(a) || !PICCOLO_IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot get remainder of %s divided by %s.", piccolo_getTypeName(b), piccolo_getTypeName(a));
                    break;
                }
                double aNum = PICCOLO_AS_NUM(a);
                double bNum = PICCOLO_AS_NUM(b);
                if(aNum == (int)aNum && bNum == (int)bNum) {
                    piccolo_enginePushStack(engine, PICCOLO_NUM_VAL((int)bNum % (int)aNum));
                    break;
                }
                piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(fmod(bNum, aNum)));
                break;
            }
            case PICCOLO_OP_EQUAL: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(piccolo_valuesEqual(a, b)));
                break;
            }
            case PICCOLO_OP_GREATER: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_NUM(a) || !PICCOLO_IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot compare %s and %s.", piccolo_getTypeName(a), piccolo_getTypeName(b));
                    break;
                }
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(PICCOLO_AS_NUM(a) < PICCOLO_AS_NUM(b)));
                break;
            }
            case PICCOLO_OP_NEGATE: {
                piccolo_Value val = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_NUM(val)) {
                    piccolo_runtimeError(engine, "Cannot negate %s.", piccolo_getTypeName(val));
                    break;
                }
                piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(-PICCOLO_AS_NUM(val)));
                break;
            }
            case PICCOLO_OP_NOT: {
                piccolo_Value val = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_BOOL(val)) {
                    piccolo_runtimeError(engine, "Cannot negate %s.", piccolo_getTypeName(val));
                    break;
                }
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(!PICCOLO_AS_BOOL(val)));
                break;
            }
            case PICCOLO_OP_LESS: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_NUM(a) || !PICCOLO_IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot compare %s and %s.", piccolo_getTypeName(a), piccolo_getTypeName(b));
                }
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(PICCOLO_AS_NUM(a) > PICCOLO_AS_NUM(b)));
                break;
            }
            case PICCOLO_OP_CREATE_ARRAY: {
                int len = READ_PARAM();
                struct piccolo_ObjArray* array = piccolo_newArray(engine, len);
                for(int i = len - 1; i >= 0; i--) {
                    array->array.values[i] = piccolo_enginePopStack(engine);
                }
                piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(array));
                break;
            }
            case PICCOLO_OP_CREATE_RANGE: {
                piccolo_Value b = piccolo_enginePopStack(engine);
                piccolo_Value a = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_NUM(a) || !PICCOLO_IS_NUM(b)) {
                    piccolo_runtimeError(engine, "Cannot create range between %s and %s.", piccolo_getTypeName(a), piccolo_getTypeName(b));
                    break;
                }
                int aNum = PICCOLO_AS_NUM(a);
                int bNum = PICCOLO_AS_NUM(b);
                struct piccolo_ObjArray* range = piccolo_newArray(engine, bNum - aNum);
                for(int i = 0; i < bNum - aNum; i++)
                    range->array.values[i] = PICCOLO_NUM_VAL(aNum + i);
                piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(range));
                break;
            }
            case PICCOLO_OP_GET_IDX: {
                piccolo_Value idx = piccolo_enginePopStack(engine);
                piccolo_Value container = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_OBJ(container)) {
                    piccolo_runtimeError(engine, "Cannot index %s", piccolo_getTypeName(container));
                } else {
                    struct piccolo_Obj* containerObj = PICCOLO_AS_OBJ(container);
                    piccolo_Value value = indexing(engine, containerObj, idx, false, PICCOLO_NIL_VAL());
                    piccolo_enginePushStack(engine, value);
                }
                break;
            }
            case PICCOLO_OP_SET_IDX: {
                piccolo_Value val = piccolo_enginePopStack(engine);
                piccolo_Value idx = piccolo_enginePopStack(engine);
                piccolo_Value container = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_OBJ(container)) {
                    piccolo_runtimeError(engine, "Cannot index %s", piccolo_getTypeName(container));
                } else {
                    struct piccolo_Obj* containerObj = PICCOLO_AS_OBJ(container);
                    indexing(engine, containerObj, idx, true, val);
                }
                piccolo_enginePushStack(engine, val);
                break;
            }
            case PICCOLO_OP_POP_STACK: {
                piccolo_enginePopStack(engine);
                break;
            }
            case PICCOLO_OP_PEEK_STACK: {
                int dist = READ_PARAM();
                piccolo_Value val = piccolo_enginePeekStack(engine, dist);
                piccolo_enginePushStack(engine, val);
                break;
            }
            case PICCOLO_OP_SWAP_STACK: {
                piccolo_Value a = piccolo_enginePopStack(engine);
                piccolo_Value b = piccolo_enginePopStack(engine);
                piccolo_enginePushStack(engine, a);
                piccolo_enginePushStack(engine, b);
                break;
            }
            case PICCOLO_OP_HASHMAP: {
                struct piccolo_ObjHashmap* hashmap = piccolo_newHashmap(engine);
                struct piccolo_HashmapValue val;
                piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(hashmap));
                break;
            }
            case PICCOLO_OP_GET_LOCAL: {
                int slot = READ_PARAM();
                int realSlot = slot + engine->frames[engine->currFrame].localStart;
                piccolo_enginePushStack(engine, engine->locals.values[realSlot]);
                break;
            }
            case PICCOLO_OP_SET_LOCAL: {
                int slot = READ_PARAM();
                int realSlot = slot + engine->frames[engine->currFrame].localStart;
                engine->locals.values[realSlot] = piccolo_enginePeekStack(engine, 1);
                break;
            }
            case PICCOLO_OP_PUSH_LOCAL: {
                piccolo_writeValueArray(engine, &engine->locals, piccolo_enginePeekStack(engine, 1));
                break;
            }
            case PICCOLO_OP_POP_LOCALS: {
                engine->locals.count -= READ_PARAM();
                break;
            }
            case PICCOLO_OP_GET_GLOBAL: {
                int slot = READ_PARAM();
                while(engine->frames[engine->currFrame].package->globals.count <= slot)
                    piccolo_writeValueArray(engine, &engine->frames[engine->currFrame].package->globals, PICCOLO_NIL_VAL());
                piccolo_enginePushStack(engine, engine->frames[engine->currFrame].package->globals.values[slot]);
                break;
            }
            case PICCOLO_OP_SET_GLOBAL: {
                int slot = READ_PARAM();
                while(engine->frames[engine->currFrame].package->globals.count <= slot)
                    piccolo_writeValueArray(engine, &engine->frames[engine->currFrame].package->globals, PICCOLO_NIL_VAL());
                engine->frames[engine->currFrame].package->globals.values[slot] = piccolo_enginePeekStack(engine, 1);
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
            case PICCOLO_OP_REV_JUMP_FALSE: {
                int jumpDist = READ_PARAM();
                piccolo_Value condition = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_BOOL(condition)) {
                    piccolo_runtimeError(engine, "Condition must be a boolean.");
                    break;
                }
                if(!PICCOLO_AS_BOOL(condition)) {
                    engine->frames[engine->currFrame].ip -= jumpDist + 3;
                }
                break;
            }
            case PICCOLO_OP_CALL: {
                int argCount = READ_PARAM();
                engine->currFrame++;
                engine->frames[engine->currFrame].localStart = engine->locals.count;
                for(int i = 0; i < argCount; i++)
                    piccolo_writeValueArray(engine, &engine->locals, PICCOLO_NIL_VAL());
                for(int i = argCount - 1; i >= 0; i--) {
                    engine->locals.values[engine->frames[engine->currFrame].localStart + i] = piccolo_enginePopStack(engine);
                }
                piccolo_Value func = piccolo_enginePopStack(engine);

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
                    engine->frames[engine->currFrame].package = closureObj->package;
                    if (funcObj->arity != argCount) {
                        engine->currFrame--;
                        piccolo_runtimeError(engine, "Wrong argument count.");
                        break;
                    }
                }
                if(type == PICCOLO_OBJ_NATIVE_FN) {
                    struct piccolo_ObjNativeFn* native = (struct piccolo_ObjNativeFn*)PICCOLO_AS_OBJ(func);
                    engine->currFrame--;
                    piccolo_enginePushStack(engine, native->native(engine, argCount, &engine->locals.values[engine->frames[engine->currFrame + 1].localStart]));
                    engine->locals.count -= argCount;
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
                        closure->upvals[i] = piccolo_newUpval(engine, &engine->locals.values[engine->frames[engine->currFrame].localStart + slot]);
                    else
                        closure->upvals[i] = engine->frames[engine->currFrame].closure->upvals[slot];
                }
                closure->package = engine->frames[engine->currFrame].package;
                piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(closure));
                break;
            }
            case PICCOLO_OP_GET_UPVAL: {
                int slot = READ_PARAM();
                piccolo_enginePushStack(engine, *engine->frames[engine->currFrame].closure->upvals[slot]->valPtr);
                break;
            }
            case PICCOLO_OP_SET_UPVAL: {
                int slot = READ_PARAM();
                *engine->frames[engine->currFrame].closure->upvals[slot]->valPtr = piccolo_enginePeekStack(engine, 1);
                break;
            }
            case PICCOLO_OP_APPEND: {
                piccolo_Value val = piccolo_enginePopStack(engine);
                struct piccolo_ObjArray* arr = (struct piccolo_ObjArray*) PICCOLO_AS_OBJ(piccolo_enginePeekStack(engine, 1));
                piccolo_writeValueArray(engine, &arr->array, val);
                break;
            }
            case PICCOLO_OP_CLOSE_UPVALS: {
                struct piccolo_ObjUpval* newOpen = NULL;
                struct piccolo_ObjUpval* curr = engine->openUpvals;
                while(curr != NULL) {
                    struct piccolo_ObjUpval* next = curr->next;
                    if(shouldCloseUpval(engine, curr)) {
                        curr->open = false;
                        piccolo_Value* heapUpval = PICCOLO_REALLOCATE("heap upval", engine, NULL, 0, sizeof(piccolo_Value));
                        *heapUpval = *curr->valPtr;
                        curr->valPtr = heapUpval;
                    } else {
                        curr->next = newOpen;
                        newOpen = curr;
                    }
                    curr = next;
                }
                engine->openUpvals = newOpen;
                break;
            }
            case PICCOLO_OP_GET_LEN: {
                piccolo_Value val = piccolo_enginePopStack(engine);
                if(PICCOLO_IS_OBJ(val)) {
                    struct piccolo_Obj* obj = PICCOLO_AS_OBJ(val);
                    if(obj->type == PICCOLO_OBJ_STRING) {
                        struct piccolo_ObjString* str = (struct piccolo_ObjString*)obj;
                        piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(str->len));
                        break;
                    }
                    if(obj->type == PICCOLO_OBJ_ARRAY) {
                        struct piccolo_ObjArray* arr = (struct piccolo_ObjArray*)obj;
                        piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(arr->array.count));
                        break;
                    }
                }
                
                piccolo_runtimeError(engine, "Cannot get length of %s.", piccolo_getTypeName(val));
                break;
            }
            case PICCOLO_OP_EXECUTE_PACKAGE: {
                piccolo_Value val = piccolo_enginePeekStack(engine, 1);
                struct piccolo_Package* package = (struct piccolo_Package*)PICCOLO_AS_OBJ(val);
                if(!package->executed && package->compiled) {
                    engine->currFrame++;
                    engine->frames[engine->currFrame].package = package;
                    engine->frames[engine->currFrame].closure = NULL;
                    package->executed = true;
                    engine->frames[engine->currFrame].ip = 0;
                    engine->frames[engine->currFrame].bytecode = &package->bytecode;
                }
                break;
            }
            default: {
                piccolo_runtimeError(engine, "Unknown opcode.");
                break;
            }
        }
#ifdef PICCOLO_ENABLE_ENGINE_DEBUG
        piccolo_disassembleInstruction(engine->frames[engine->currFrame].bytecode, engine->frames[engine->currFrame].prevIp);
        printf("DATA STACK:\n");
        for(piccolo_Value* stackPtr = engine->stack; stackPtr != engine->stackTop; stackPtr++) {
            printf("[");
            piccolo_printValue(*stackPtr);
            printf("] ");
        }
        printf("\n");
        printf("LOCAL STACK:\n");
        for(int i = 0; i < engine->localCnt; i++) {
            printf("[");
            piccolo_printValue(engine->locals[i]);
            printf("] ");
        }
        printf("\n");
#endif
        if(engine->liveMemory > engine->gcThreshold) {
            piccolo_collectGarbage(engine);
            engine->gcThreshold = engine->liveMemory * 2;
        }
        if(engine->hadError) {
            return false;
        }
    }
#undef READ_BYTE
}

bool piccolo_executePackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    engine->currFrame++;
    engine->frames[engine->currFrame].package = package;
    package->executed = true;
    bool result = piccolo_executeBytecode(engine, &package->bytecode);
    engine->currFrame--;
    return result;
}

bool piccolo_executeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode) {
    engine->frames[engine->currFrame].localStart = 0;
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
    piccolo_Value value = piccolo_enginePeekStack(engine, 1);
    engine->stackTop--;
    return value;
}

piccolo_Value piccolo_enginePeekStack(struct piccolo_Engine* engine, int dist) {
    return *(engine->stackTop - dist);
}

void piccolo_runtimeError(struct piccolo_Engine* engine, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
    piccolo_enginePrintError(engine, " [%s]\n", engine->frames[engine->currFrame].package->packageName);

    int charIdx = engine->frames[engine->currFrame].bytecode->charIdxs.values[engine->frames[engine->currFrame].prevIp];
    struct piccolo_strutil_LineInfo opLine = piccolo_strutil_getLine(engine->frames[engine->currFrame].package->source, charIdx);
    piccolo_enginePrintError(engine, "[line %d] %.*s\n", opLine.line + 1, opLine.lineEnd - opLine.lineStart, opLine.lineStart);

    int lineNumberDigits = 0;
    int lineNumber = opLine.line + 1;
    while(lineNumber > 0) {
        lineNumberDigits++;
        lineNumber /= 10;
    }
    piccolo_enginePrintError(engine, "%*c ^", 7 + lineNumberDigits + engine->frames[engine->currFrame].package->source + charIdx - opLine.lineStart, ' ');
    piccolo_enginePrintError(engine, "\n");

    engine->hadError = true;
}

#ifdef PICCOLO_ENABLE_MEMORY_TRACKER

#include <stdlib.h>
void piccolo_freeMemTracks(struct piccolo_Engine* engine) {
    struct piccolo_MemoryTrack* track = engine->track;
    while(track != NULL) {
        struct piccolo_MemoryTrack* toFree = track;
        track = track->next;
        free(toFree);
    }
}

#endif

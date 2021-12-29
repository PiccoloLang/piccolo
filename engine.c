
#include "engine.h"

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "util/strutil.h"
#include "object.h"
#include "gc.h"
#include "limits.h"

//#define PICCOLO_ENABLE_ENGINE_DEBUG

#ifdef PICCOLO_ENABLE_ENGINE_DEBUG
#include <stdio.h>
#include "debug/disassembler.h"
#endif

#define PICCOLO_MAX_FRAMES 1024

PICCOLO_DYNARRAY_IMPL(struct piccolo_Package*, Package)
PICCOLO_DYNARRAY_IMPL(const char*, String)
PICCOLO_DYNARRAY_IMPL(struct piccolo_CallFrame, CallFrame)

void piccolo_initEngine(struct piccolo_Engine* engine, void (*printError)(const char* format, va_list)) {
    piccolo_initValueArray(&engine->locals);
    engine->printError = printError;
    engine->stackTop = engine->stack;
    engine->openUpvals = NULL;
    engine->liveMemory = 0;
    engine->gcThreshold = 1024 * 64;
    engine->objs = NULL;
    piccolo_initPackageArray(&engine->packages);
    piccolo_initCallFrameArray(&engine->callFrames);
    piccolo_initStringArray(&engine->searchPaths);
    piccolo_initValueArray(&engine->locals);
#ifdef PICCOLO_ENABLE_MEMORY_TRACKER
    engine->track = NULL;
#endif
}

#define CURR_FRAME engine->callFrames.values[engine->callFrames.count - 1]

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

    piccolo_freeValueArray(engine, &engine->locals);
    piccolo_freeCallFrameArray(engine, &engine->callFrames);
    piccolo_freeStringArray(engine, &engine->searchPaths);
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
                if(PICCOLO_IS_OBJ(idx) && PICCOLO_AS_OBJ(idx)->type == PICCOLO_OBJ_STRING) {
                    struct piccolo_ObjString* str = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(idx);
                    if(str->len == 6 && strcmp(str->string, "length") == 0) {
                        if(set) {
                            piccolo_runtimeError(engine, "Cannot set string length.");
                        } else {
                            return PICCOLO_NUM_VAL(string->utf8Len);
                        }
                    } else {
                        piccolo_runtimeError(engine, "Cannot index string with %s.", piccolo_getTypeName(idx));
                        break;
                    }
                } else {
                    piccolo_runtimeError(engine, "Cannot index string with %s.", piccolo_getTypeName(idx));
                    break;
                }
                return PICCOLO_NIL_VAL();
            }
            int idxNum = PICCOLO_AS_NUM(idx);
            if(idxNum < 0 || idxNum >= string->len) {
                piccolo_runtimeError(engine, "Index %d out of bounds.", idxNum);
                return PICCOLO_NIL_VAL();
            }

            const char* curr = string->string;
            for(int i = 0; i < idxNum; i++) {
                curr += piccolo_strutil_utf8Chars(*curr);
            }
            char str[5];

            int charCnt = piccolo_strutil_utf8Chars(*curr);
            for(int i = 0; i < charCnt; i++)
                str[i] = curr[i];
            str[charCnt] = '\0';
            struct piccolo_ObjString* result = piccolo_copyString(engine, str, charCnt);
            return  PICCOLO_OBJ_VAL(result);
        }
        case PICCOLO_OBJ_ARRAY: {
            struct piccolo_ObjArray* array = (struct piccolo_ObjArray*)container;
            if(!PICCOLO_IS_NUM(idx)) {
                if(PICCOLO_IS_OBJ(idx) && PICCOLO_AS_OBJ(idx)->type == PICCOLO_OBJ_STRING) {
                    struct piccolo_ObjString* str = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(idx);
                    if(str->len == 6 && strcmp(str->string, "length") == 0) {
                        if(set) {
                            piccolo_runtimeError(engine, "Cannot set array length.");
                            break;
                        } else {
                            return PICCOLO_NUM_VAL(array->array.count);
                        }
                    } else {
                        piccolo_runtimeError(engine, "Cannot index array with %s.", piccolo_getTypeName(idx));
                        break;
                    }
                } else {
                    piccolo_runtimeError(engine, "Cannot index array with %s.", piccolo_getTypeName(idx));
                    break;
                }
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
        case PICCOLO_OBJ_NATIVE_STRUCT: {
            struct piccolo_ObjNativeStruct* nativeStruct = (struct piccolo_ObjNativeStruct*)container;
            if(nativeStruct->index != NULL) {
                return nativeStruct->index(PICCOLO_GET_PAYLOAD(nativeStruct, void), engine, idx, set, value);
            } else {
                piccolo_runtimeError(engine, "Cannot index %s", nativeStruct->typename);
            }
            break;
        }
        default: {
            piccolo_runtimeError(engine, "Cannot index %s", piccolo_getTypeName(PICCOLO_OBJ_VAL(container)));
            return PICCOLO_NIL_VAL();
        }
    }

    // This could maybe be marked unreachable since the default case returns but there is no way to handle compiler intrinsic atm.
    // TODO: Possibly a future compiler_features.h or the like could resolve this kind of thing
    return PICCOLO_NIL_VAL();
}

static bool shouldCloseUpval(struct piccolo_Engine* engine, struct piccolo_ObjUpval* upval) {
    return upval->valPtr >= engine->locals.values + engine->locals.count;
}

static void pushFrame(struct piccolo_Engine* engine) {
    struct piccolo_CallFrame frame = { 0 };
    frame.closure = NULL;
    piccolo_writeCallFrameArray(engine, &engine->callFrames, frame);
}

static void popFrame(struct piccolo_Engine* engine) {
    engine->callFrames.count--;
}

static bool run(struct piccolo_Engine* engine) {
#define READ_BYTE() (CURR_FRAME.bytecode->code.values[CURR_FRAME.ip++])
#define READ_PARAM() ((READ_BYTE() << 8) + READ_BYTE())
    engine->hadError = false;
    while(true) {
        if(engine->callFrames.capacity >= PICCOLO_MAX_FRAMES) {
            // TODO: Nicer output when encountering invalid call frame state
            piccolo_enginePrintError(engine, "Call frame depth exceeded limit (%i).\n", PICCOLO_MAX_FRAMES);
            break;
        }
        if(!engine->callFrames.values) {
            piccolo_enginePrintError(engine, "Invalid callframe state encountered.\n");
            break;
        }
        CURR_FRAME.prevIp = CURR_FRAME.ip;
        enum piccolo_OpCode opcode = READ_BYTE();
        switch(opcode) {
            case PICCOLO_OP_RETURN:
                popFrame(engine);
                if(engine->callFrames.count == 0)
                    return true;
                CURR_FRAME.prevIp = CURR_FRAME.ip;
                break;
            case PICCOLO_OP_CONST: {
                piccolo_enginePushStack(engine, CURR_FRAME.bytecode->constants.values[READ_PARAM()]);
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
                        if(PICCOLO_AS_NUM(b) == INFINITY) {
                            piccolo_runtimeError(engine, "Cannot multiply string by INFINITY.");
                            break;
                        }
                        repetitions = PICCOLO_AS_NUM(b);
                        string = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(a);
                    }
                    if(repetitions < 0) {
                        piccolo_runtimeError(engine, "Can't multiply string by negative number.");
                        break;
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
                        if(PICCOLO_AS_NUM(a) > INT_MAX || PICCOLO_AS_NUM(a) < INT_MIN) {
                            piccolo_runtimeError(engine, "Array repetition exceeded integer limits.");
                            break;
                        }
                        repetitions = PICCOLO_AS_NUM(a);
                        array = (struct piccolo_ObjArray*)PICCOLO_AS_OBJ(b);
                    } else {
                        if(PICCOLO_AS_NUM(b) > INT_MAX || PICCOLO_AS_NUM(b) < INT_MIN) {
                            piccolo_runtimeError(engine, "Array repetition exceeded integer limits.");
                            break;
                        }
                        repetitions = PICCOLO_AS_NUM(b);
                        array = (struct piccolo_ObjArray*)PICCOLO_AS_OBJ(a);
                    }

                    double newCount = (double) array->array.count * (double) repetitions;
                    if(newCount > INT_MAX || newCount < INT_MIN) {
                        piccolo_runtimeError(engine, "Array repetition exceeded integer limits.");
                        break;
                    }

                    struct piccolo_ObjArray* result = piccolo_newArray(engine, (int) newCount);
                    if(!result->array.values) {
                        piccolo_runtimeError(engine, "Failed to allocate for new array.");
                        break;
                    }
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
                // TODO: Very jank but will do for now
                if(aNum < INT_MAX && aNum > INT_MIN && bNum < INT_MAX && bNum > INT_MIN && aNum == (int)aNum && bNum == (int)bNum) {
                    if(aNum == 0) {
                        piccolo_runtimeError(engine, "Divide by zero.");
                        break;
                    }
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
                if(!PICCOLO_AS_NUM(val)) {
                    piccolo_runtimeError(engine, "Cannot negate nil.");
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
                double aDouble = PICCOLO_AS_NUM(a);
                double bDouble = PICCOLO_AS_NUM(b);
                if(aDouble > INT_MAX || aDouble < INT_MIN ||
                   bDouble > INT_MAX || bDouble < INT_MIN) {
                    piccolo_runtimeError(engine, "Range limits too large");
                    break;
                }
                int aNum = (int)aDouble;
                int bNum = (int)bDouble;
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
                    break;
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
                    break;
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
                int realSlot = slot + CURR_FRAME.localStart;
                piccolo_enginePushStack(engine, engine->locals.values[realSlot]);
                break;
            }
            case PICCOLO_OP_SET_LOCAL: {
                int slot = READ_PARAM();
                int realSlot = slot + CURR_FRAME.localStart;
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
                while(CURR_FRAME.package->globals.count <= slot)
                    piccolo_writeValueArray(engine, &CURR_FRAME.package->globals, PICCOLO_NIL_VAL());
                piccolo_enginePushStack(engine, CURR_FRAME.package->globals.values[slot]);
                break;
            }
            case PICCOLO_OP_SET_GLOBAL: {
                int slot = READ_PARAM();
                while(CURR_FRAME.package->globals.count <= slot)
                    piccolo_writeValueArray(engine, &CURR_FRAME.package->globals, PICCOLO_NIL_VAL());
                CURR_FRAME.package->globals.values[slot] = piccolo_enginePeekStack(engine, 1);
                break;
            }
            case PICCOLO_OP_JUMP: {
                int jumpDist = READ_PARAM();
                CURR_FRAME.ip += jumpDist - 3;
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
                    CURR_FRAME.ip += jumpDist - 3;
                }
                break;
            }
            case PICCOLO_OP_REV_JUMP: {
                int jumpDist = READ_PARAM();
                CURR_FRAME.ip -= jumpDist + 3;
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
                    CURR_FRAME.ip -= jumpDist + 3;
                }
                break;
            }
            case PICCOLO_OP_CALL: {
                int argCount = READ_PARAM();
                pushFrame(engine);
                CURR_FRAME.localStart = engine->locals.count;
                for(int i = 0; i < argCount; i++)
                    piccolo_writeValueArray(engine, &engine->locals, PICCOLO_NIL_VAL());
                for(int i = argCount; i > 0; i--) {
                    piccolo_Value arg = piccolo_enginePopStack(engine);
                    engine->locals.values[CURR_FRAME.localStart + (i - 1)] = arg;
                }
                piccolo_Value func = piccolo_enginePopStack(engine);

                if(engine->callFrames.count == PICCOLO_MAX_FRAMES) {
                    popFrame(engine);
                    piccolo_runtimeError(engine, "Recursion stack overflow.");
                    break;
                }

                if(!PICCOLO_IS_OBJ(func) || (PICCOLO_AS_OBJ(func)->type != PICCOLO_OBJ_CLOSURE && PICCOLO_AS_OBJ(func)->type != PICCOLO_OBJ_NATIVE_FN)) {
                    popFrame(engine);
                    piccolo_runtimeError(engine, "Cannot call %s.", piccolo_getTypeName(func));
                    break;
                }
                enum piccolo_ObjType type = PICCOLO_AS_OBJ(func)->type;

                if(type == PICCOLO_OBJ_CLOSURE) {
                    struct piccolo_ObjClosure* closureObj = (struct piccolo_ObjClosure*)PICCOLO_AS_OBJ(func);
                    struct piccolo_ObjFunction* funcObj = closureObj->prototype;
                    CURR_FRAME.ip = CURR_FRAME.prevIp = 0;
                    CURR_FRAME.bytecode = &funcObj->bytecode;
                    CURR_FRAME.closure = closureObj;
                    CURR_FRAME.package = closureObj->package;
                    if (funcObj->arity != argCount) {
                        popFrame(engine);
                        piccolo_runtimeError(engine, "Wrong argument count.");
                        break;
                    }
                }
                if(type == PICCOLO_OBJ_NATIVE_FN) {
                    struct piccolo_ObjNativeFn* native = (struct piccolo_ObjNativeFn*)PICCOLO_AS_OBJ(func);
                    popFrame(engine);
                    if(argCount == 0) {
                        piccolo_enginePushStack(engine, native->native(engine, 0, NULL, native->self));
                        break;
                    }
                    piccolo_enginePushStack(engine, native->native(engine, argCount, &engine->locals.values[engine->callFrames.values[engine->callFrames.count].localStart], native->self));
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
                        closure->upvals[i] = piccolo_newUpval(engine, &engine->locals.values[CURR_FRAME.localStart + slot]);
                    else
                        closure->upvals[i] = CURR_FRAME.closure->upvals[slot];
                }
                closure->package = CURR_FRAME.package;
                piccolo_enginePushStack(engine, PICCOLO_OBJ_VAL(closure));
                break;
            }
            case PICCOLO_OP_GET_UPVAL: {
                int slot = READ_PARAM();
                piccolo_enginePushStack(engine, *CURR_FRAME.closure->upvals[slot]->valPtr);
                break;
            }
            case PICCOLO_OP_SET_UPVAL: {
                int slot = READ_PARAM();
                *CURR_FRAME.closure->upvals[slot]->valPtr = piccolo_enginePeekStack(engine, 1);
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
                        piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(str->utf8Len));
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
            case PICCOLO_OP_IN: {
                piccolo_Value container = piccolo_enginePopStack(engine);
                piccolo_Value key = piccolo_enginePopStack(engine);
                if(!PICCOLO_IS_OBJ(container) || PICCOLO_AS_OBJ(container)->type != PICCOLO_OBJ_HASHMAP) {
                    piccolo_runtimeError(engine, "Container must be a hashmap.");
                    break;
                }
                struct piccolo_ObjHashmap* hashmap = (struct piccolo_ObjHashmap*)PICCOLO_AS_OBJ(container);
                struct piccolo_HashmapValue entry = piccolo_getHashmap(engine, &hashmap->hashmap, key);
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(entry.exists));
                break;
            }
            case PICCOLO_OP_ITER_FIRST: {
                piccolo_Value container = piccolo_enginePeekStack(engine, 1);
                if(!PICCOLO_IS_OBJ(container)) {
                    piccolo_runtimeError(engine, "Cannot iterate over %s.", piccolo_getTypeName(container));
                    break;
                }
                if(PICCOLO_AS_OBJ(container)->type != PICCOLO_OBJ_ARRAY &&
                   PICCOLO_AS_OBJ(container)->type != PICCOLO_OBJ_STRING &&
                   PICCOLO_AS_OBJ(container)->type != PICCOLO_OBJ_HASHMAP) {
                    piccolo_runtimeError(engine, "Cannot iterate over %s.", piccolo_getTypeName(container));
                    break;
                }
                struct piccolo_Obj* containerObj = PICCOLO_AS_OBJ(container);
                if(containerObj->type == PICCOLO_OBJ_ARRAY || containerObj->type == PICCOLO_OBJ_STRING)
                    piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(0));
                if(containerObj->type == PICCOLO_OBJ_HASHMAP) {
                    int idx = 0;
                    struct piccolo_ObjHashmap* hashmap = (struct piccolo_ObjHashmap*)containerObj;
                    while(idx < hashmap->hashmap.capacity && !hashmap->hashmap.entries[idx].val.exists)
                        idx++;
                    piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(idx));
                }
                break;
            }
            case PICCOLO_OP_ITER_CONT: {
                piccolo_Value iterator = piccolo_enginePopStack(engine);
                piccolo_Value container = piccolo_enginePopStack(engine);
                struct piccolo_Obj* containerObj = PICCOLO_AS_OBJ(container);
                int idx = PICCOLO_AS_NUM(iterator);
                bool last;
                switch(containerObj->type) {
                    case PICCOLO_OBJ_ARRAY: {
                        last = idx >= ((struct piccolo_ObjArray*)containerObj)->array.count;
                        break;
                    }
                    case PICCOLO_OBJ_STRING: {
                        last = idx >= ((struct piccolo_ObjString*)containerObj)->len;
                        break;
                    }
                    case PICCOLO_OBJ_HASHMAP: {
                        last = idx >= ((struct piccolo_ObjHashmap*)containerObj)->hashmap.capacity;
                        break;
                    }
                    case PICCOLO_OBJ_FUNC: {
                        piccolo_runtimeError(engine, "Invalid operand to iterator.");
                        break;
                    }
                    case PICCOLO_OBJ_UPVAL: {
                        piccolo_runtimeError(engine, "Invalid operand to iterator.");
                        break;
                    }
                    case PICCOLO_OBJ_CLOSURE: {
                        piccolo_runtimeError(engine, "Invalid operand to iterator.");
                        break;
                    }
                    case PICCOLO_OBJ_NATIVE_FN: {
                        piccolo_runtimeError(engine, "Invalid operand to iterator.");
                        break;
                    }
                    case PICCOLO_OBJ_PACKAGE: {
                        piccolo_runtimeError(engine, "Invalid operand to iterator.");
                        break;
                    }
                }
                piccolo_enginePushStack(engine, PICCOLO_BOOL_VAL(!last));
                break;
            }
            case PICCOLO_OP_ITER_NEXT: {
                piccolo_Value container = piccolo_enginePeekStack(engine, READ_PARAM());
                piccolo_Value iterator = piccolo_enginePopStack(engine);
                struct piccolo_Obj* containerObj = PICCOLO_AS_OBJ(container);
                int idx = PICCOLO_AS_NUM(iterator);
                if(containerObj->type == PICCOLO_OBJ_ARRAY)
                    idx++;
                if(containerObj->type == PICCOLO_OBJ_STRING) {
                    struct piccolo_ObjString* str = (struct piccolo_ObjString*)containerObj;
                    idx += piccolo_strutil_utf8Chars(str->string[idx]);
                }
                if(containerObj->type == PICCOLO_OBJ_HASHMAP) {
                    idx++;
                    while(idx < ((struct piccolo_ObjHashmap*)containerObj)->hashmap.capacity && !((struct piccolo_ObjHashmap*)containerObj)->hashmap.entries[idx].val.exists)
                        idx++;
                }
                piccolo_enginePushStack(engine, PICCOLO_NUM_VAL(idx));
                break;
            }
            case PICCOLO_OP_ITER_GET: {
                int idx = PICCOLO_AS_NUM(piccolo_enginePopStack(engine));
                struct piccolo_Obj* container = PICCOLO_AS_OBJ(piccolo_enginePopStack(engine));
                piccolo_Value val;
                if(container->type == PICCOLO_OBJ_ARRAY)
                    val = indexing(engine, container, PICCOLO_NUM_VAL(idx), false, PICCOLO_NIL_VAL());
                if(container->type == PICCOLO_OBJ_STRING) {
                    struct piccolo_ObjString* string = (struct piccolo_ObjString*)container;
                    char str[5];
                    int charCnt = piccolo_strutil_utf8Chars(string->string[idx]);
                    memcpy(str, &string->string[idx], charCnt);
                    str[charCnt] = '\0';
                    struct piccolo_ObjString* result = piccolo_copyString(engine, str, charCnt);
                    val = PICCOLO_OBJ_VAL(result);
                }
                if(container->type == PICCOLO_OBJ_HASHMAP)
                    val = ((struct piccolo_ObjHashmap*)container)->hashmap.entries[idx].key;
                piccolo_enginePushStack(engine, val);
                break;
            }
            case PICCOLO_OP_EXECUTE_PACKAGE: {
                piccolo_Value val = piccolo_enginePeekStack(engine, 1);
                struct piccolo_Package* package = (struct piccolo_Package*)PICCOLO_AS_OBJ(val);
                if(!package->executed && package->compiled) {
                    pushFrame(engine);
                    CURR_FRAME.package = package;
                    CURR_FRAME.closure = NULL;
                    CURR_FRAME.ip = 0;
                    CURR_FRAME.bytecode = &package->bytecode;
                    CURR_FRAME.localStart = engine->locals.count;
                    package->executed = true;
                }
                break;
            }
            default: {
                piccolo_runtimeError(engine, "Unknown opcode.");
                break;
            }
        }
#ifdef PICCOLO_ENABLE_ENGINE_DEBUG
        piccolo_disassembleInstruction(CURR_FRAME.bytecode, CURR_FRAME.prevIp);
        printf("DATA STACK:\n");
        for(piccolo_Value* stackPtr = engine->stack; stackPtr != engine->stackTop; stackPtr++) {
            printf("[");
            piccolo_printValue(*stackPtr);
            printf("] ");
        }
        printf("\n");
        printf("LOCAL STACK:\n");
        for(int i = 0; i < engine->locals.count; i++) {
            printf("[");
            piccolo_printValue(engine->locals.values[i]);
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

    return false;
}

bool piccolo_executePackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    pushFrame(engine);
    CURR_FRAME.closure = NULL;
    CURR_FRAME.package = package;
    package->executed = true;
    bool result = piccolo_executeBytecode(engine, &package->bytecode);
    return result;
}

bool piccolo_executeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode) {
    CURR_FRAME.localStart = 0;
    CURR_FRAME.ip = 0;
    CURR_FRAME.bytecode = bytecode;
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
    piccolo_enginePrintError(engine, " [%s]\n", CURR_FRAME.package->packageName);

    int charIdx = CURR_FRAME.bytecode->charIdxs.values[CURR_FRAME.prevIp];
    struct piccolo_strutil_LineInfo opLine = piccolo_strutil_getLine(CURR_FRAME.package->source, charIdx);
    piccolo_enginePrintError(engine, "[line %d] %.*s\n", opLine.line + 1, opLine.lineEnd - opLine.lineStart, opLine.lineStart);

    int lineNumberDigits = 0;
    int lineNumber = opLine.line + 1;
    while(lineNumber > 0) {
        lineNumberDigits++;
        lineNumber /= 10;
    }
    piccolo_enginePrintError(engine, "%*c ^", 7 + lineNumberDigits + CURR_FRAME.package->source + charIdx - opLine.lineStart, ' ');
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

#undef CURR_FRAME


#include "gc.h"
#include <stdio.h>

static void markObj(struct piccolo_Obj* obj) {
    if(obj == NULL)
        return;
    if(obj->marked)
        return;
    obj->marked = true;
    switch(obj->type) {
        case PICCOLO_OBJ_ARRAY: {
            struct piccolo_ObjArray* array = (struct piccolo_ObjArray*)obj;
            for(int i = 0; i < array->array.count; i++)
                piccolo_gcMarkValue(array->array.values[i]);
            break;
        }
        case PICCOLO_OBJ_HASHMAP: {
            struct piccolo_ObjHashmap* hashmap = (struct piccolo_ObjHashmap*)obj;
            for(int i = 0; i < hashmap->hashmap.capacity; i++) {
                if(!piccolo_HashmapIsBaseKey(hashmap->hashmap.entries[i].key)) {
                    piccolo_gcMarkValue(hashmap->hashmap.entries[i].key);
                    piccolo_gcMarkValue(hashmap->hashmap.entries[i].val.value);
                }
            }
            break;
        }
        case PICCOLO_OBJ_FUNC: {
            struct piccolo_ObjFunction* func = (struct piccolo_ObjFunction*)obj;
            for(int i = 0; i < func->bytecode.constants.count; i++)
                piccolo_gcMarkValue(func->bytecode.constants.values[i]);
            break;
        }
        case PICCOLO_OBJ_UPVAL: {
            struct piccolo_ObjUpval* upval = (struct piccolo_ObjUpval*)obj;
            piccolo_gcMarkValue(*upval->valPtr);
            break;
        }
        case PICCOLO_OBJ_CLOSURE: {
            struct piccolo_ObjClosure* closure = (struct piccolo_ObjClosure*)obj;
            markObj((struct piccolo_Obj*) closure->prototype);
            for(int i = 0; i < closure->upvalCnt; i++) {
                markObj((struct piccolo_Obj*) closure->upvals[i]);
            }
            break;
        }
        case PICCOLO_OBJ_NATIVE_STRUCT: {
            struct piccolo_ObjNativeStruct* nativeStruct = (struct piccolo_ObjNativeStruct*)obj;
            if(nativeStruct->gcMark != NULL)
                nativeStruct->gcMark(PICCOLO_GET_PAYLOAD(obj, void));
            break;
        }
        case PICCOLO_OBJ_STRING: break;
        case PICCOLO_OBJ_NATIVE_FN: break;
        case PICCOLO_OBJ_PACKAGE: break;
    }
}

void piccolo_gcMarkValue(piccolo_Value value) {
    if(PICCOLO_IS_OBJ(value)) {
        markObj(PICCOLO_AS_OBJ(value));
    }
}

static void markPackage(struct piccolo_Package* package) {
    package->obj.marked = true;
    for(int i = 0; i < package->bytecode.constants.count; i++)
        piccolo_gcMarkValue(package->bytecode.constants.values[i]);
    for(int i = 0; i < package->globals.count; i++)
        piccolo_gcMarkValue(package->globals.values[i]);
    for(int i = 0; i < package->globalIdxs.capacity; i++)
        if(package->globalIdxs.entries[i].key != NULL)
            package->globalIdxs.entries[i].key->obj.marked = true;
}

static void markRoots(struct piccolo_Engine* engine) {
    for(piccolo_Value* iter = engine->stack; iter != engine->stackTop; iter++)
        piccolo_gcMarkValue(*iter);
    for(int i = 0; i < engine->callFrames.count; i++) {
        if(engine->callFrames.values[i].closure != NULL)
            markObj((struct piccolo_Obj*)engine->callFrames.values[i].closure);
    }
    for(int i = 0; i < engine->locals.count; i++)
        piccolo_gcMarkValue(engine->locals.values[i]);
    for(int i = 0; i < engine->packages.count; i++)
        markPackage(engine->packages.values[i]);
}

void piccolo_collectGarbage(struct piccolo_Engine* engine) {

    struct piccolo_Obj* currObj = engine->objs;
    while(currObj != NULL) {
        currObj->marked = false;
        currObj = currObj->next;
    }
    markRoots(engine);

    struct piccolo_Obj* newObjs = NULL;
    currObj = engine->objs;
    int liveObjs = 0;
    while(currObj != NULL) {
        struct piccolo_Obj* curr = currObj;
        currObj = currObj->next;
        if(curr->marked) {
            liveObjs++;
            curr->next = newObjs;
            newObjs = curr;
        } else {
            piccolo_freeObj(engine, curr);
        }
    }

    engine->objs = newObjs;
}
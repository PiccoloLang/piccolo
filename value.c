
#include <stdio.h>
#include "value.h"
#include "object.h"
#include "package.h"
#include "debug/disassembler.h"

#include <string.h>

PICCOLO_DYNARRAY_IMPL(piccolo_Value, Value)

static void printObject(struct piccolo_Obj* obj) {
    bool printed = obj->printed;
    obj->printed = true;
    if(obj->type == PICCOLO_OBJ_STRING) {
        struct piccolo_ObjString* string = (struct piccolo_ObjString*)obj;
        printf("%.*s", string->len, string->string);
    }
    if(obj->type == PICCOLO_OBJ_ARRAY) {
        struct piccolo_ObjArray* array = (struct piccolo_ObjArray*)obj;
        printf("[");
        if(!printed) {
            for(int i = 0; i < array->array.count; i++) {
                piccolo_printValue(array->array.values[i]);
                if(i < array->array.count - 1)
                    printf(", ");
            }
        } else {
            printf("...");
        }
        printf("]");
    }
    if(obj->type == PICCOLO_OBJ_HASHMAP) {
        struct piccolo_ObjHashmap* hashmap = (struct piccolo_ObjHashmap*)obj;
        printf("{");
        if(!printed) {
            bool first = true;
            for(int i = 0; i < hashmap->hashmap.capacity; i++) {
                if(!piccolo_HashmapIsBaseKey(hashmap->hashmap.entries[i].key)) {
                    if(!first)
                        printf(", ");
                    piccolo_printValue(hashmap->hashmap.entries[i].key);
                    printf(": ");
                    piccolo_printValue(hashmap->hashmap.entries[i].val.value);
                    first = false;
                }
            }
        } else {
            printf("...");
        }
        printf("}");
    }
    if(obj->type == PICCOLO_OBJ_FUNC) {
        printf("<fn>");
    }
    if(obj->type == PICCOLO_OBJ_CLOSURE) {
        printf("<fn>");
    }
    if(obj->type == PICCOLO_OBJ_NATIVE_FN) {
        printf("<native fn>");
    }
    if(obj->type == PICCOLO_OBJ_PACKAGE) {
        struct piccolo_Package* package = ((struct piccolo_Package*)obj);
        printf("<package \"%s\">", package->packageName);
    }
    if(obj->type == PICCOLO_OBJ_NATIVE_STRUCT) {
        struct piccolo_ObjNativeStruct* nativeStruct = (struct piccolo_ObjNativeStruct*)obj;
        printf("<%s>", nativeStruct->Typename);
    }
    obj->printed = false;
}

void piccolo_printValue(piccolo_Value value) {
    if(PICCOLO_IS_NIL(value)) {
        printf("nil");
        return;
    }
    if(PICCOLO_IS_NUM(value)) {
        printf("%g", PICCOLO_AS_NUM(value));
        return;
    }
    if(PICCOLO_IS_BOOL(value)) {
        printf(PICCOLO_AS_BOOL(value) ? "true" : "false");
        return;
    }
    if(PICCOLO_IS_OBJ(value)) {
        printObject(PICCOLO_AS_OBJ(value));
    }
}

char* piccolo_getTypeName(piccolo_Value value) {
    if(PICCOLO_IS_NIL(value)) {
        return "nil";
    }
    if(PICCOLO_IS_NUM(value)) {
        return "num";
    }
    if(PICCOLO_IS_BOOL(value)) {
        return "bool";
    }
    if(PICCOLO_IS_OBJ(value)) {
        enum piccolo_ObjType type = PICCOLO_AS_OBJ(value)->type;
        if(type == PICCOLO_OBJ_STRING)
            return "str";
        if(type == PICCOLO_OBJ_ARRAY)
            return "array";
        if(type == PICCOLO_OBJ_HASHMAP)
            return "hashmap";
        if(type == PICCOLO_OBJ_FUNC)
            return "raw fn";
        if(type == PICCOLO_OBJ_CLOSURE)
            return "fn";
        if(type == PICCOLO_OBJ_NATIVE_FN)
            return "native fn";
        if(type == PICCOLO_OBJ_PACKAGE)
            return "package";
    }
    return "Unknown";
}

bool piccolo_valuesEqual(struct piccolo_Value a, struct piccolo_Value b) {
    if(PICCOLO_IS_NUM(a) && PICCOLO_IS_NUM(b)) {
        return PICCOLO_AS_NUM(a) == PICCOLO_AS_NUM(b);
    }
    if(PICCOLO_IS_BOOL(a) && PICCOLO_IS_BOOL(b)) {
        return PICCOLO_AS_BOOL(a) == PICCOLO_AS_BOOL(b);
    }
    if(PICCOLO_IS_NIL(a) && PICCOLO_IS_NIL(b)) {
        return true;
    }
    if(PICCOLO_IS_OBJ(a) && PICCOLO_IS_OBJ(b)) {
        struct piccolo_Obj* aObj = PICCOLO_AS_OBJ(a);
        struct piccolo_Obj* bObj = PICCOLO_AS_OBJ(b);
        if(aObj->type == PICCOLO_OBJ_STRING && bObj->type == PICCOLO_OBJ_STRING) {
            struct piccolo_ObjString* aStr = (struct piccolo_ObjString*)aObj;
            struct piccolo_ObjString* bStr = (struct piccolo_ObjString*)bObj;
            if(aStr->len == bStr->len && strncmp(aStr->string, bStr->string, aStr->len) == 0) {
                return true;
            }
        }
    }
    return false;
}

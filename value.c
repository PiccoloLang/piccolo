
#include <stdio.h>
#include "value.h"
#include "object.h"
#include "package.h"
#include "debug/disassembler.h"

PICCOLO_DYNARRAY_IMPL(piccolo_Value, Value)

static void printObject(struct piccolo_Obj* obj) {
    if(obj->type == PICCOLO_OBJ_STRING) {
        struct piccolo_ObjString* string = (struct piccolo_ObjString*)obj;
        printf("%.*s", string->len, string->string);
    }
    if(obj->type == PICCOLO_OBJ_ARRAY) {
        struct piccolo_ObjArray* array = (struct piccolo_ObjArray*)obj;
        printf("[");
        for(int i = 0; i < array->array.count; i++) {
            piccolo_printValue(array->array.values[i]);
            if(i < array->array.count - 1)
                printf(", ");
        }
        printf("]");
    }
    if(obj->type == PICCOLO_OBJ_FUNC) {
        printf("<fn>");
    }
    if(obj->type == PICCOLO_OBJ_CLOSURE) {
        printf("<fn>");
    }
    if(obj->type == PICCOLO_OBJ_PACKAGE) {
        struct piccolo_Package* package = ((struct piccolo_Package*)obj);
        printf("<package \"%s\">", package->packageName);
    }
}

void piccolo_printValue(piccolo_Value value) {
    if(PICCOLO_IS_NIL(value)) {
        printf("nil");
        return;
    }
    if(PICCOLO_IS_NUM(value)) {
        printf("%f", PICCOLO_AS_NUM(value));
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
        return "number";
    }
    if(PICCOLO_IS_BOOL(value)) {
        return "bool";
    }
    if(PICCOLO_IS_OBJ(value)) {
        enum piccolo_ObjType type = PICCOLO_AS_OBJ(value)->type;
        if(type == PICCOLO_OBJ_STRING)
            return "string";
        if(type == PICCOLO_OBJ_ARRAY)
            return "array";
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
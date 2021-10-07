
#include <stdio.h>
#include "value.h"
#include "object.h"

PICCOLO_DYNARRAY_IMPL(piccolo_Value, Value)

static void printObject(struct piccolo_Obj* obj) {
    if(obj->type == PICCOLO_OBJ_STRING) {
        struct piccolo_ObjString* string = (struct piccolo_ObjString*)obj;
        printf("%.*s", string->len, string->string);
    }
    if(obj->type == PICCOLO_OBJ_FUNC) {
        printf("<fn>");
    }
    if(obj->type == PICCOLO_OBJ_CLOSURE) {
        printf("<fn>");
    }
}

void piccolo_printValue(piccolo_Value value) {
    if(IS_NIL(value)) {
        printf("nil");
        return;
    }
    if(IS_NUM(value)) {
        printf("%f", AS_NUM(value));
        return;
    }
    if(IS_BOOL(value)) {
        printf(AS_BOOL(value) ? "true" : "false");
        return;
    }
    if(IS_PTR(value)) {
        printf("<ptr>");
    }
    if(IS_OBJ(value)) {
        printObject(AS_OBJ(value));
    }
}

char* piccolo_getTypeName(piccolo_Value value) {
    if(IS_NIL(value)) {
        return "nil";
    }
    if(IS_NUM(value)) {
        return "number";
    }
    if(IS_BOOL(value)) {
        return "bool";
    }
    if(IS_PTR(value)) {
        return "ptr";
    }
    if(IS_OBJ(value)) {
        enum piccolo_ObjType type = AS_OBJ(value)->type;
        if(type == PICCOLO_OBJ_STRING)
            return "string";
        if(type == PICCOLO_OBJ_FUNC)
            return "raw fn";
        if(type == PICCOLO_OBJ_CLOSURE)
            return "fn";
        if(type == PICCOLO_OBJ_NATIVE_FN)
            return "native fn";
    }
    return "Unknown";
}
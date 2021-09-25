
#include <stdio.h>
#include "value.h"
#include "object.h"
#include "debug/disassembler.h"

PICCOLO_DYNARRAY_IMPL(piccolo_Value, Value)

static void printObject(struct piccolo_Obj* obj) {
    if(obj->type == PICCOLO_OBJ_FUNC) {
        printf("<fn %d>", ((struct piccolo_ObjFunction*)obj)->arity);
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
    return "Unknown";
}
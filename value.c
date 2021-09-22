
#include <stdio.h>
#include "value.h"

PICCOLO_DYNARRAY_IMPL(piccolo_Value, Value)

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
    return "Unknown";
}
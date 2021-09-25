
#ifndef PICCOLO_VALUE_H
#define PICCOLO_VALUE_H

#include <stdbool.h>

#include "util/dynarray.h"

enum piccolo_ValueType {
    PICCOLO_VALUE_NIL,
    PICCOLO_VALUE_NUMBER,
    PICCOLO_VALUE_BOOL,
    PICCOLO_VALUE_PTR,
    PICCOLO_VALUE_OBJ,
};

struct piccolo_Value {
    enum piccolo_ValueType type;
    union {
        double number;
        bool boolean;
        struct piccolo_Value* ptr;
        struct piccolo_Obj* obj;
    } as;
};

typedef struct piccolo_Value piccolo_Value;

#define IS_NIL(value) (value.type == PICCOLO_VALUE_NIL)
#define IS_NUM(value) (value.type == PICCOLO_VALUE_NUMBER)
#define IS_BOOL(value) (value.type == PICCOLO_VALUE_BOOL)
#define IS_PTR(value) (value.type == PICCOLO_VALUE_PTR)
#define IS_OBJ(value) (value.type == PICCOLO_VALUE_OBJ)

#define AS_NUM(value) (value.as.number)
#define AS_BOOL(value) (value.as.boolean)
#define AS_PTR(value) (value.as.ptr)
#define AS_OBJ(value) (value.as.obj)

#define NIL_VAL() ((piccolo_Value){PICCOLO_VALUE_NIL, {.number = 0}})
#define NUM_VAL(num) ((piccolo_Value){PICCOLO_VALUE_NUMBER, {.number = (num)}})
#define BOOL_VAL(bool) ((piccolo_Value){PICCOLO_VALUE_BOOL, {.boolean = (bool)}})
#define PTR_VAL(pointer) ((piccolo_Value){PICCOLO_VALUE_PTR, {.ptr = (pointer)}})
#define OBJ_VAL(object)((piccolo_Value){PICCOLO_VALUE_OBJ, {.obj = (object)}})

PICCOLO_DYNARRAY_HEADER(piccolo_Value, Value)

void piccolo_printValue(piccolo_Value value);
char* piccolo_getTypeName(piccolo_Value value);

#endif

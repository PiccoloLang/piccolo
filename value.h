
#ifndef PICCOLO_VALUE_H
#define PICCOLO_VALUE_H

#include <stdbool.h>

#include "util/dynarray.h"

enum piccolo_ValueType {
    PICCOLO_VALUE_NIL,
    PICCOLO_VALUE_NUMBER,
    PICCOLO_VALUE_BOOL
};

typedef struct {
    enum piccolo_ValueType type;
    union {
        double number;
        bool boolean;
    } as;
} piccolo_Value;

#define IS_NIL(value) (value.type == PICCOLO_VALUE_NIL)
#define IS_NUM(value) (value.type == PICCOLO_VALUE_NUMBER)
#define IS_BOOL(value) (value.type == PICCOLO_VALUE_BOOL)

#define AS_NUM(value) (value.as.number)
#define AS_BOOL(value) (value.as.boolean)

#define NIL_VAL() ((piccolo_Value){PICCOLO_VALUE_NIL, {.number = 0}})
#define NUM_VAL(num) ((piccolo_Value){PICCOLO_VALUE_NUMBER, {.number = num}})
#define BOOL_VAL(bool) ((piccolo_Value){PICCOLO_VALUE_BOOL, {.boolean = bool}})

PICCOLO_DYNARRAY_HEADER(piccolo_Value, Value)

void piccolo_printValue(piccolo_Value value);
char* piccolo_getTypeName(piccolo_Value value);

#endif

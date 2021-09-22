
#include <stdio.h>
#include "value.h"

PICCOLO_DYNARRAY_IMPL(piccolo_Value, Value)

void printValue(piccolo_Value value) {
    printf("%f", value);
}
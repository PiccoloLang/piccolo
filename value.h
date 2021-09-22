
#ifndef PICCOLO_VALUE_H
#define PICCOLO_VALUE_H

#include "util/dynarray.h"

typedef double piccolo_Value;

PICCOLO_DYNARRAY_HEADER(piccolo_Value, Value)

void printValue(piccolo_Value value);

#endif

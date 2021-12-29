
#ifndef PICCOLO_GC_H
#define PICCOLO_GC_H

#include "engine.h"

void piccolo_gcMarkValue(piccolo_Value value);
void piccolo_collectGarbage(struct piccolo_Engine* engine);

#endif


#ifndef PICCOLO_STDLIB_H
#define PICCOLO_STDLIB_H

#include "../engine.h"

void piccolo_addIOLib(struct piccolo_Engine* engine);
void piccolo_addTimeLib(struct piccolo_Engine* engine);
void piccolo_addMathLib(struct piccolo_Engine* engine);
void piccolo_addRandomLib(struct piccolo_Engine* engine);
void piccolo_addStrLib(struct piccolo_Engine* engine);
void piccolo_addFileLib(struct piccolo_Engine* engine);
void piccolo_addDLLLib(struct piccolo_Engine* engine);
void piccolo_addOSLib(struct piccolo_Engine* engine);

#endif

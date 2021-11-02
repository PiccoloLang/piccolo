
#ifndef PICCOLO_STDLIB_H
#define PICCOLO_STDLIB_H

#include "../engine.h"

void piccolo_addIOLib(struct piccolo_Engine* engine);
void piccolo_addTimeLib(struct piccolo_Engine* engine);

#ifdef PICCOLO_ENABLE_DEBUG_LIB

void piccolo_addDebugLib(struct piccolo_Engine* engine);

#endif

#endif

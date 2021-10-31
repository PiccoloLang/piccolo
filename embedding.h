
#ifndef PICCOLO_EMBEDDING_H
#define PICCOLO_EMBEDDING_H

#include "engine.h"

void piccolo_defineGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen, struct piccolo_Value value);
void piccolo_defineGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, struct piccolo_Value value);

#endif

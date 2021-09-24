
#ifndef PICCOLO_EMBEDDING_H
#define PICCOLO_EMBEDDING_H

#include "engine.h"

piccolo_Value piccolo_getGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen);
piccolo_Value piccolo_getGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name);

#endif


#ifndef PICCOLO_EMBEDDING_H
#define PICCOLO_EMBEDDING_H

#include "engine.h"
#include "gc.h"
#include "typecheck.h"

void piccolo_addSearchPath(struct piccolo_Engine* engine, const char* path);
void piccolo_defineGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen, struct piccolo_Value value, struct piccolo_Type* type);
void piccolo_defineGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, struct piccolo_Value value);
void piccolo_defineGlobalWithType(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, struct piccolo_Value value, struct piccolo_Type* type);

piccolo_Value piccolo_getGlobalWithNameSize(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name, size_t nameLen);
piccolo_Value piccolo_getGlobal(struct piccolo_Engine* engine, struct piccolo_Package* package, const char* name);

struct piccolo_Type* piccolo_makeFnType(struct piccolo_Engine* engine, struct piccolo_Type* resultType, int nParams, ...);

#endif

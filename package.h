
#ifndef PICCOLO_PACKAGE_H
#define PICCOLO_PACKAGE_H

#include "bytecode.h"

struct piccolo_Package {
    struct piccolo_Bytecode bytecode;
    char* source;
};

struct piccolo_Engine;

struct piccolo_Package* piccolo_loadPackage(struct piccolo_Engine* engine, const char* filepath);
void piccolo_freePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);

#endif

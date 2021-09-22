
#ifndef PICCOLO_ENGINE_H
#define PICCOLO_ENGINE_H

#include "package.h"

struct piccolo_Engine {
    struct piccolo_Package package;

    struct piccolo_Bytecode* bytecode;
    uint8_t* ip;
    struct piccolo_ValueArray stack;

    void (*printError)(const char* format, ...);
};

void piccolo_initEngine(struct piccolo_Engine* engine, void (*printError)(const char* format, ...));
void piccolo_freeEngine(struct piccolo_Engine* engine);
void piccolo_executeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode);

void piccolo_enginePushStack(struct piccolo_Engine* engine, piccolo_Value value);
piccolo_Value piccolo_enginePopStack(struct piccolo_Engine* engine);

#endif

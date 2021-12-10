
#ifndef PICCOLO_ENGINE_H
#define PICCOLO_ENGINE_H

#include "package.h"
#include "object.h"
#include <stdarg.h>
#include <stdbool.h>

struct piccolo_CallFrame {
    int localStart;
    int prevIp;
    int ip;
    struct piccolo_Bytecode* bytecode;
    struct piccolo_ObjClosure* closure;
    struct piccolo_Package* package;
};

PICCOLO_DYNARRAY_HEADER(struct piccolo_Package*, Package)

struct piccolo_Engine {
    struct piccolo_PackageArray packages;

    piccolo_Value locals[256];
    int localCnt;
    piccolo_Value stack[256];
    piccolo_Value* stackTop;
    struct piccolo_CallFrame frames[256];
    int currFrame;
    bool hadError;

    size_t liveMemory;
    size_t gcThreshold;
    struct piccolo_Obj* objs;

    void (*printError)(const char* format, va_list);

    struct piccolo_ObjUpval* openUpvals;
#ifdef PICCOLO_ENABLE_MEMORY_TRACKER
    struct piccolo_MemoryTrack* track;
#endif
};

void piccolo_initEngine(struct piccolo_Engine* engine, void (*printError)(const char* format, va_list));
void piccolo_freeEngine(struct piccolo_Engine* engine);

bool piccolo_executePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);
bool piccolo_executeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode);

void piccolo_enginePrintError(struct piccolo_Engine* engine, const char* format, ...);

void piccolo_enginePushStack(struct piccolo_Engine* engine, piccolo_Value value);
piccolo_Value piccolo_enginePopStack(struct piccolo_Engine* engine);
piccolo_Value piccolo_enginePeekStack(struct piccolo_Engine* engine, int dist);

void piccolo_runtimeError(struct piccolo_Engine* engine, const char* format, ...);

#ifdef PICCOLO_ENABLE_MEMORY_TRACKER
void piccolo_freeMemTracks(struct piccolo_Engine* engine);
#endif

#endif

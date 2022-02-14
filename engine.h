
#ifndef PICCOLO_ENGINE_H
#define PICCOLO_ENGINE_H

#include "package.h"
#include "object.h"
#include "typecheck.h"
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
PICCOLO_DYNARRAY_HEADER(const char*, String)
PICCOLO_DYNARRAY_HEADER(struct piccolo_CallFrame, CallFrame)

struct piccolo_Engine {
    struct piccolo_PackageArray packages;

    struct piccolo_ValueArray locals;
    piccolo_Value stack[256];
    piccolo_Value* stackTop;
    struct piccolo_CallFrameArray callFrames;
    bool hadError;

    size_t liveMemory;
    size_t gcThreshold;
    struct piccolo_Obj* objs;

    void (*printError)(const char* format, va_list);

    struct piccolo_Package* (*findPackage)(struct piccolo_Engine*, struct piccolo_Compiler* compiler, const char* sourceFilepath, const char* name, size_t nameLen);

    struct piccolo_ObjUpval* openUpvals;

    struct piccolo_StringArray searchPaths;
    struct piccolo_Type* types;
#ifdef PICCOLO_ENABLE_MEMORY_TRACKER
    struct piccolo_MemoryTrack* track;
#endif
};

void piccolo_initEngine(struct piccolo_Engine* engine, void (*printError)(const char* format, va_list));
void piccolo_freeEngine(struct piccolo_Engine* engine);

bool piccolo_executePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);
bool piccolo_executeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode);
piccolo_Value piccolo_callFunction(struct piccolo_Engine* engine, struct piccolo_ObjClosure* closure, int argc, piccolo_Value* argv);

void piccolo_enginePrintError(struct piccolo_Engine* engine, const char* format, ...);

void piccolo_enginePushStack(struct piccolo_Engine* engine, piccolo_Value value);
piccolo_Value piccolo_enginePopStack(struct piccolo_Engine* engine);
piccolo_Value piccolo_enginePeekStack(struct piccolo_Engine* engine, int dist);

void piccolo_runtimeError(struct piccolo_Engine* engine, const char* format, ...);

#ifdef PICCOLO_ENABLE_MEMORY_TRACKER
void piccolo_freeMemTracks(struct piccolo_Engine* engine);
#endif

#endif

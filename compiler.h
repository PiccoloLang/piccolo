
#ifndef PICCOLO_COMPILER_H
#define PICCOLO_COMPILER_H

#include <stdbool.h>

#include "scanner.h"
#include "util/dynarray.h"
#include "typecheck.h"
#include "bytecode.h"
#include "parser.h"

// Maximum length of a package name
#define PICCOLO_MAX_PACKAGE 4096

struct piccolo_Engine;
struct piccolo_Package;

struct piccolo_Variable {
    int slot;
    const char* nameStart;
    size_t nameLen;
    bool Mutable;
    struct puccolo_ExprNode* decl;
};

struct piccolo_Upvalue {
    int slot;
    bool local;
    bool Mutable;
    struct piccolo_VarDeclNode* decl;
};

struct piccolo_VarData {
    int slot;
    enum piccolo_OpCode setOp;
    enum piccolo_OpCode getOp;
    bool mutable;
    struct piccolo_VarDeclNode* decl;
};

PICCOLO_DYNARRAY_HEADER(struct piccolo_Variable, Variable)
PICCOLO_DYNARRAY_HEADER(struct piccolo_Upvalue, Upvalue)

struct piccolo_Compiler {
    struct piccolo_Package* package;
    struct piccolo_VariableArray* globals;
    struct piccolo_VariableArray locals;
    struct piccolo_UpvalueArray upvals;
    struct piccolo_Compiler* enclosing;
    bool hadError;
};

struct piccolo_Package* piccolo_resolvePackage(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, const char* sourceFilepath, const char* name, size_t nameLen);
struct piccolo_Package* piccolo_resolveImport(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, const char* sourceFilepath, struct piccolo_ImportNode* import);

void piccolo_compilationError(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, int charIdx, const char* format, ...);
struct piccolo_VarData piccolo_getVariable(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Token name);
bool piccolo_compilePackage(struct piccolo_Engine* engine, struct piccolo_Package* package);

#endif


#include "compiler.h"

#include <stdlib.h>
#include <stdarg.h>

#include "bytecode.h"
#include "package.h"
#include "engine.h"
#include "util/strutil.h"
#include "value.h"

static void compilationError(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
    piccolo_enginePrintError(engine, "\n");
    struct piccolo_strutil_LineInfo tokenLine = piccolo_strutil_getLine(compiler->scanner.source, compiler->current.charIdx);
    piccolo_enginePrintError(engine, "[line %d] %.*s\n", tokenLine.line + 1, tokenLine.lineEnd - tokenLine.lineStart, tokenLine.lineStart);
    int lineNumberDigits = 0;
    int lineNumber = tokenLine.line + 1;
    while(lineNumber > 0) {
        lineNumberDigits++;
        lineNumber /= 10;
    }
    piccolo_enginePrintError(engine, "%* ^", 9 + lineNumberDigits + compiler->current.start - tokenLine.lineStart);
    piccolo_enginePrintError(engine, "\n");
    compiler->hadError = true;
}

static void advanceCompiler(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler) {
    compiler->current = piccolo_nextToken(&compiler->scanner);
    while(compiler->current.type == PICCOLO_TOKEN_ERROR) {
        compilationError(engine, compiler, "Malformed token");
        compiler->current = piccolo_nextToken(&compiler->scanner);
    }
}

static void compileExpr(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Bytecode* bytecode);

static void compileLiteral(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Bytecode* bytecode) {
    if(compiler->current.type == PICCOLO_TOKEN_NUM) {
        piccolo_writeConst(engine, bytecode, NUM_VAL(strtod(compiler->current.start, NULL)), compiler->current.charIdx);
        advanceCompiler(engine, compiler);
        return;
    }
    if(compiler->current.type == PICCOLO_TOKEN_NIL) {
        piccolo_writeConst(engine, bytecode, NIL_VAL(), compiler->current.charIdx);
        advanceCompiler(engine, compiler);
        return;
    }
    if(compiler->current.type == PICCOLO_TOKEN_LEFT_PAREN) {
        advanceCompiler(engine, compiler);
        compileExpr(engine, compiler, bytecode);
        if(compiler->current.type != PICCOLO_TOKEN_RIGHT_PAREN) {
            compilationError(engine, compiler, "Expected ).");
        } else {
            advanceCompiler(engine, compiler);
        }
        return;
    }

    compilationError(engine, compiler, "Expected expression.");
}

static void compileUnary(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Bytecode* bytecode) {
    if(compiler->current.type == PICCOLO_TOKEN_MINUS) {
        int charIdx = compiler->current.charIdx;
        piccolo_writeConst(engine, bytecode, NUM_VAL(0), charIdx);
        advanceCompiler(engine, compiler);
        compileUnary(engine, compiler, bytecode);
        piccolo_writeBytecode(engine, bytecode, OP_SUB, charIdx);
        return;
    }
    compileLiteral(engine, compiler, bytecode);
}

static void compileMultiplicative(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Bytecode* bytecode) {
    compileUnary(engine, compiler, bytecode);
    if(compiler->current.type == PICCOLO_TOKEN_STAR || compiler->current.type == PICCOLO_TOKEN_SLASH) {
        enum piccolo_TokenType operation = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileMultiplicative(engine, compiler, bytecode);
        if(operation == PICCOLO_TOKEN_STAR)
            piccolo_writeBytecode(engine, bytecode, OP_MUL, charIdx);
        if(operation == PICCOLO_TOKEN_SLASH)
            piccolo_writeBytecode(engine, bytecode, OP_DIV, charIdx);
    }
}

static void compileAdditive(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Bytecode* bytecode) {
    compileMultiplicative(engine, compiler, bytecode);
    if(compiler->current.type == PICCOLO_TOKEN_PLUS || compiler->current.type == PICCOLO_TOKEN_MINUS) {
        enum piccolo_TokenType operation = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileAdditive(engine, compiler, bytecode);
        if(operation == PICCOLO_TOKEN_PLUS)
            piccolo_writeBytecode(engine, bytecode, OP_ADD, charIdx);
        if(operation == PICCOLO_TOKEN_MINUS)
            piccolo_writeBytecode(engine, bytecode, OP_SUB, charIdx);
    }
}

static void compileExpr(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Bytecode* bytecode) {
    compileAdditive(engine, compiler, bytecode);
}

static void initCompiler(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, char* source) {
    piccolo_initScanner(&compiler->scanner, source);
    advanceCompiler(engine, compiler);
}

bool piccolo_compilePackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    struct piccolo_Compiler compiler;
    initCompiler(engine, &compiler, package->source);

    compileExpr(engine, &compiler, &package->bytecode);
    if(compiler.current.type != PICCOLO_TOKEN_EOF) {
        compilationError(engine, &compiler, "Expected EOF.");
    }

    piccolo_writeBytecode(engine, &package->bytecode, OP_RETURN, 1);
    return !compiler.hadError;
}
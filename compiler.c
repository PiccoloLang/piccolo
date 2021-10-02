
#include "compiler.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "bytecode.h"
#include "package.h"
#include "engine.h"
#include "util/strutil.h"
#include "value.h"
#include "object.h"

PICCOLO_DYNARRAY_IMPL(struct piccolo_Variable, Variable)

static void freeCompiler(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler) {
    piccolo_freeVariableArray(engine, &compiler->locals);
}

static void compilationError(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
    piccolo_enginePrintError(engine, "\n");
    struct piccolo_strutil_LineInfo tokenLine = piccolo_strutil_getLine(compiler->scanner->source, compiler->current.charIdx);
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
    compiler->current = piccolo_nextToken(compiler->scanner);
    while(compiler->current.type == PICCOLO_TOKEN_ERROR) {
        compilationError(engine, compiler, "Malformed token");
        compiler->current = piccolo_nextToken(compiler->scanner);
    }
    compiler->cycled = false;
}

static int getVarSlot(struct piccolo_VariableArray* variables, struct piccolo_Token token) {
    for(int i = 0; i < variables->count; i++)
        if(variables->values[i].nameLen == token.length && memcmp(token.start, variables->values[i].name, token.length) == 0)
            return i;
    return -1;
}

#define COMPILE_PARAMETERS struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Bytecode* bytecode, bool requireValue, bool global
#define COMPILE_ARGUMENTS engine, compiler, bytecode, requireValue, global
#define COMPILE_ARGUMENTS_REQ_VAL engine, compiler, bytecode, true, global
#define COMPILE_ARGUMENTS_NO_VAL engine, compiler, bytecode, false, global

static void compileExpr(COMPILE_PARAMETERS);

static void compileLiteral(COMPILE_PARAMETERS) {
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
    if(compiler->current.type == PICCOLO_TOKEN_FALSE) {
        piccolo_writeConst(engine, bytecode, BOOL_VAL(false), compiler->current.charIdx);
        advanceCompiler(engine, compiler);
        return;
    }
    if(compiler->current.type == PICCOLO_TOKEN_TRUE) {
        piccolo_writeConst(engine, bytecode, BOOL_VAL(true), compiler->current.charIdx);
        advanceCompiler(engine, compiler);
        return;
    }
    if(compiler->current.type == PICCOLO_TOKEN_LEFT_PAREN) {
        advanceCompiler(engine, compiler);
        compileExpr(COMPILE_ARGUMENTS_REQ_VAL);
        if(compiler->current.type != PICCOLO_TOKEN_RIGHT_PAREN) {
            compilationError(engine, compiler, "Expected ).");
        } else {
            advanceCompiler(engine, compiler);
        }
        return;
    }

    // The reason for this "cycled" nonsense is that in Piccolo,
    // all statements are expressions. That means that in a
    // program like this:
    // print 5 + print 5
    // the recursive descent parser needs to go all the way back
    // the top for the second print. However, we need to prevent
    // infinite recursion. Hence, the cycled argument.
    if(!compiler->cycled) {
        compiler->cycled = true;
        compileExpr(COMPILE_ARGUMENTS);
        return;
    }

    compilationError(engine, compiler, "Expected expression.");
    advanceCompiler(engine, compiler);
}

static void compileFnLiteral(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_FN) {
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);

        struct piccolo_Compiler functionCompiler;
        functionCompiler.scanner = compiler->scanner;
        functionCompiler.globals = compiler->globals;
        piccolo_initVariableArray(&functionCompiler.locals);

        struct piccolo_ObjFunction* function = piccolo_newFunction(engine);

        while(compiler->current.type != PICCOLO_TOKEN_ARROW) {
            if(compiler->current.type == PICCOLO_TOKEN_EOF) {
                compilationError(engine, compiler, "Expected ->.");
                return;
            }
            struct piccolo_Token parameterName = compiler->current;
            if(compiler->current.type != PICCOLO_TOKEN_IDENTIFIER) {
                compilationError(engine, compiler, "Expected parameter name.");
            } else {
                struct piccolo_Variable parameter;
                parameter.name = parameterName.start;
                parameter.nameLen = parameterName.length;
                parameter.slot = functionCompiler.locals.count;
                piccolo_writeVariableArray(engine, &functionCompiler.locals, parameter);
                function->arity++;
            }
            advanceCompiler(engine, compiler);
        }

        advanceCompiler(engine, compiler);
        functionCompiler.current = compiler->current;
        functionCompiler.cycled = false;
        functionCompiler.hadError = false;

        compileExpr(engine, &functionCompiler, &function->bytecode, true, false);
        piccolo_writeBytecode(engine, &function->bytecode, OP_RETURN, 1);
        freeCompiler(engine, &functionCompiler);

        if(functionCompiler.hadError)
            compiler->hadError = true;

        compiler->current = functionCompiler.current;

        piccolo_writeConst(engine, bytecode, OBJ_VAL(function), charIdx);

        return;
    }
    compileLiteral(COMPILE_ARGUMENTS);
}

static void compileVarLookup(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_IDENTIFIER) {
        struct piccolo_Token varName = compiler->current;
        int globalSlot = getVarSlot(compiler->globals, varName);
        if(globalSlot == -1) {
            int localSlot = getVarSlot(&compiler->locals, varName);
            if(localSlot == -1) {
                compilationError(engine, compiler, "Variable %.*s is not defined.", varName.length, varName.start);
            } else {
                piccolo_writeParameteredBytecode(engine, bytecode, OP_GET_STACK, localSlot, varName.charIdx);
            }
        } else {
            piccolo_writeParameteredBytecode(engine, bytecode, OP_GET_GLOBAL, globalSlot, varName.charIdx);
        }
        advanceCompiler(engine, compiler);
        return;
    }
    compileFnLiteral(COMPILE_ARGUMENTS);
}

static void compileFnCall(COMPILE_PARAMETERS) {
    compileVarLookup(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_LEFT_PAREN) {
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        int args = 0;
        while(compiler->current.type != PICCOLO_TOKEN_RIGHT_PAREN) {
            if(compiler->current.type == PICCOLO_TOKEN_EOF) {
                compilationError(engine, compiler, "Expected ).");
                break;
            }
            compileExpr(COMPILE_ARGUMENTS_REQ_VAL);
            if(compiler->current.type != PICCOLO_TOKEN_RIGHT_PAREN && compiler->current.type != PICCOLO_TOKEN_COMMA)
                compilationError(engine, compiler, "Expected comma.");
            if(compiler->current.type == PICCOLO_TOKEN_COMMA)
                advanceCompiler(engine, compiler);

            args++;
        }
        advanceCompiler(engine, compiler);
        piccolo_writeParameteredBytecode(engine, bytecode, OP_CALL, args, charIdx);
    }
}

static void compileUnary(COMPILE_PARAMETERS) {
    while(compiler->current.type == PICCOLO_TOKEN_MINUS || compiler->current.type == PICCOLO_TOKEN_BANG) {
        enum piccolo_TokenType op = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        if(op == PICCOLO_TOKEN_MINUS)
            piccolo_writeConst(engine, bytecode, NUM_VAL(0), charIdx);
        compileFnCall(COMPILE_ARGUMENTS_REQ_VAL);
        if(op == PICCOLO_TOKEN_MINUS)
            piccolo_writeBytecode(engine, bytecode, OP_SUB, charIdx);
        if(op == PICCOLO_TOKEN_BANG)
            piccolo_writeBytecode(engine, bytecode, OP_NOT, charIdx);
        return;
    }
    compileFnCall(COMPILE_ARGUMENTS);
}

static void compileMultiplicative(COMPILE_PARAMETERS) {
    compileUnary(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_STAR || compiler->current.type == PICCOLO_TOKEN_SLASH) {
        enum piccolo_TokenType operation = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileUnary(COMPILE_ARGUMENTS_REQ_VAL);
        if(operation == PICCOLO_TOKEN_STAR)
            piccolo_writeBytecode(engine, bytecode, OP_MUL, charIdx);
        if(operation == PICCOLO_TOKEN_SLASH)
            piccolo_writeBytecode(engine, bytecode, OP_DIV, charIdx);
    }
}

static void compileAdditive(COMPILE_PARAMETERS) {
    compileMultiplicative(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_PLUS || compiler->current.type == PICCOLO_TOKEN_MINUS) {
        enum piccolo_TokenType operation = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileMultiplicative(COMPILE_ARGUMENTS_REQ_VAL);
        if(operation == PICCOLO_TOKEN_PLUS)
            piccolo_writeBytecode(engine, bytecode, OP_ADD, charIdx);
        if(operation == PICCOLO_TOKEN_MINUS)
            piccolo_writeBytecode(engine, bytecode, OP_SUB, charIdx);
    }
}

static void compileComparison(COMPILE_PARAMETERS) {
    compileAdditive(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_GREATER || compiler->current.type == PICCOLO_TOKEN_LESS ||
          compiler->current.type == PICCOLO_TOKEN_GREATER_EQ || compiler->current.type == PICCOLO_TOKEN_LESS_EQ) {
        enum piccolo_TokenType operation = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileAdditive(COMPILE_ARGUMENTS_REQ_VAL);
        if(operation == PICCOLO_TOKEN_GREATER)
            piccolo_writeBytecode(engine, bytecode, OP_GREATER, charIdx);
        if(operation == PICCOLO_TOKEN_LESS)
            piccolo_writeBytecode(engine, bytecode, OP_LESS, charIdx);
        if(operation == PICCOLO_TOKEN_GREATER_EQ) {
            piccolo_writeBytecode(engine, bytecode, OP_LESS, charIdx);
            piccolo_writeBytecode(engine, bytecode, OP_NOT, charIdx);
        }
        if(operation == PICCOLO_TOKEN_LESS_EQ) {
            piccolo_writeBytecode(engine, bytecode, OP_GREATER, charIdx);
            piccolo_writeBytecode(engine, bytecode, OP_NOT, charIdx);
        }
    }
}

static void compileEquality(COMPILE_PARAMETERS) {
    compileComparison(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_EQ_EQ) {
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileComparison(COMPILE_ARGUMENTS_REQ_VAL);
        piccolo_writeBytecode(engine, bytecode, OP_EQUAL, charIdx);
    }
}

static void compileBoolOperations(COMPILE_PARAMETERS) {
    compileEquality(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_AND || compiler->current.type == PICCOLO_TOKEN_OR) {
        enum piccolo_TokenType op = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        if(op == PICCOLO_TOKEN_OR) {
            piccolo_writeBytecode(engine, bytecode, OP_NOT, charIdx);
        }
        int shortCircuitJumpAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, OP_JUMP_FALSE, 0, charIdx);
        compileEquality(COMPILE_ARGUMENTS_REQ_VAL);
        if(op == PICCOLO_TOKEN_OR)
            piccolo_writeBytecode(engine, bytecode, OP_NOT, charIdx);
        int skipValueJumpAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, OP_JUMP, 0, charIdx);
        int valueAddr = bytecode->code.count;
        piccolo_writeConst(engine, bytecode, BOOL_VAL(op == PICCOLO_TOKEN_OR), charIdx);
        int endAddr = bytecode->code.count;

        piccolo_patchParam(bytecode, shortCircuitJumpAddr, valueAddr - shortCircuitJumpAddr);
        piccolo_patchParam(bytecode, skipValueJumpAddr, endAddr - skipValueJumpAddr);
    }
}

static void compileVarSet(COMPILE_PARAMETERS) {
    int charIdx = compiler->current.charIdx;
    compileBoolOperations(COMPILE_ARGUMENTS);
    if(compiler->current.type == PICCOLO_TOKEN_EQ) {
        charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileVarSet(COMPILE_ARGUMENTS_REQ_VAL);
        piccolo_writeBytecode(engine, bytecode, OP_SET, charIdx);
        if(!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
            piccolo_writeBytecode(engine, bytecode, OP_POP_STACK, charIdx);
    } else {
        if(!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
            piccolo_writeBytecode(engine, bytecode, OP_POP_STACK, charIdx);
    }
}

static void compileVarDecl(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_VAR) {
        advanceCompiler(engine, compiler);
        struct piccolo_Token varName = compiler->current;

        int slot;
        if(compiler->current.type != PICCOLO_TOKEN_IDENTIFIER) {
            compilationError(engine, compiler, "Expected variable name.");
        } else {
            if(global) {
                slot = getVarSlot(compiler->globals, varName);
                if(slot != -1) {
                    compilationError(engine, compiler, "Variable %.*s already defined.", varName.length, varName.start);
                } else {
                    slot = compiler->globals->count;
                    piccolo_writeParameteredBytecode(engine, bytecode, OP_GET_GLOBAL, slot, varName.charIdx);
                }
            } else {
                slot = getVarSlot(&compiler->locals, varName);
                if(slot != -1) {
                    compilationError(engine, compiler, "Variable %.*s already defined.", varName.length, varName.start);
                } else {
                    slot = compiler->locals.count;
                    piccolo_writeParameteredBytecode(engine, bytecode, OP_GET_STACK, slot, varName.charIdx);
                }
            }
        }

        advanceCompiler(engine, compiler);
        int eqCharIdx = compiler->current.charIdx;
        if(compiler->current.type != PICCOLO_TOKEN_EQ) {
            compilationError(engine, compiler, "Expected =.");
        }
        advanceCompiler(engine, compiler);
        compileExpr(COMPILE_ARGUMENTS_REQ_VAL);
        piccolo_writeBytecode(engine, bytecode, OP_SET, eqCharIdx);

        if(global) {
            struct piccolo_Variable variable;
            variable.slot = slot;
            variable.name = varName.start;
            variable.nameLen = varName.length;
            piccolo_writeVariableArray(engine, compiler->globals, variable);
        } else {
            struct piccolo_Variable variable;
            variable.slot = slot;
            variable.name = varName.start;
            variable.nameLen = varName.length;
            piccolo_writeVariableArray(engine, &compiler->locals, variable);
        }

        if(!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
            piccolo_writeBytecode(engine, bytecode, OP_POP_STACK, eqCharIdx);
        return;
    }
    compileVarSet(COMPILE_ARGUMENTS);
}

static void compileIf(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_IF) {
        advanceCompiler(engine, compiler);
        int charIdx = compiler->current.charIdx;
        compileExpr(COMPILE_ARGUMENTS_REQ_VAL);
        int ifStartAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, OP_JUMP_FALSE, 0, charIdx);
        compileExpr(COMPILE_ARGUMENTS_REQ_VAL);

        int ifEndAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, OP_JUMP, 0, charIdx);
        int elseStartAddr = bytecode->code.count;

        if(compiler->current.type == PICCOLO_TOKEN_ELSE) {
            advanceCompiler(engine, compiler);
            compileExpr(COMPILE_ARGUMENTS_REQ_VAL);
        } else {
            piccolo_writeConst(engine, bytecode, NIL_VAL(), charIdx);
        }

        int elseEndAddr = bytecode->code.count;

        piccolo_patchParam(bytecode, ifStartAddr, elseStartAddr - ifStartAddr);
        piccolo_patchParam(bytecode, ifEndAddr, elseEndAddr - ifEndAddr);

        if(!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
            piccolo_writeBytecode(engine, bytecode, OP_POP_STACK, charIdx);
        return;
    }
    compileVarDecl(COMPILE_ARGUMENTS);
}

static void compileBlock(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_LEFT_BRACE) {
        int localsBefore = compiler->locals.count;
        advanceCompiler(engine, compiler);
        while(compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE) {
            if(compiler->current.type == PICCOLO_TOKEN_EOF) {
                compilationError(engine, compiler, "Expected }.");
                break;
            }
            compileVarDecl(engine, compiler, bytecode, false, false);
        }
        advanceCompiler(engine, compiler);
        int charIdx = compiler->current.charIdx;
        if(!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
            piccolo_writeBytecode(engine, bytecode, OP_POP_STACK, charIdx);
        compiler->locals.count = localsBefore;
        return;
    }
    compileIf(COMPILE_ARGUMENTS);
}

static void compileExpr(COMPILE_PARAMETERS) {
    if(requireValue) {
        compileBlock(COMPILE_ARGUMENTS_REQ_VAL);
    } else {
        while(compiler->current.type != PICCOLO_TOKEN_EOF && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE) {
            compileBlock(COMPILE_ARGUMENTS);
        }
    }
}

static void initCompiler(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, char* source) {
    piccolo_initScanner(compiler->scanner, source);
    piccolo_initVariableArray(&compiler->locals);
    advanceCompiler(engine, compiler);
}

bool piccolo_compilePackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    struct piccolo_Compiler compiler;
    struct piccolo_Scanner scanner;
    compiler.scanner = &scanner;
    initCompiler(engine, &compiler, package->source);
    compiler.globals = &package->globalVars;

    compileExpr(engine, &compiler, &package->bytecode, false, true);
    if(compiler.current.type != PICCOLO_TOKEN_EOF) {
        compilationError(engine, &compiler, "Expected EOF.");
    }

    freeCompiler(engine, &compiler);

    piccolo_writeBytecode(engine, &package->bytecode, OP_RETURN, 1);
    return !compiler.hadError;
}

#undef COMPILE_PARAMETERS
#undef COMPILE_ARGUMENTS
#undef COMPILE_ARGUMENTS_REQ_VAL
#undef COMPILE_ARGUMENTS_NO_VAL
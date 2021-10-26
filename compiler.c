
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
PICCOLO_DYNARRAY_IMPL(struct piccolo_Upvalue, Upvalue)


static void freeCompiler(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler) {
    piccolo_freeVariableArray(engine, &compiler->locals);
    piccolo_freeUpvalueArray(engine, &compiler->upvals);
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

static void initCompiler(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, char* source, bool skipScanner) {
    if(!skipScanner)
        piccolo_initScanner(compiler->scanner, source);
    piccolo_initVariableArray(&compiler->locals);
    piccolo_initUpvalueArray(&compiler->upvals);
    if(!skipScanner)
        advanceCompiler(engine, compiler);
}

static int getVarSlot(struct piccolo_VariableArray* variables, struct piccolo_Token token) {
    for(int i = 0; i < variables->count; i++)
        if(variables->values[i].nameLen == token.length && memcmp(token.start, variables->values[i].name, token.length) == 0)
            return i;
    return -1;
}

static int resolveUpvalue(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Token name) {
    if(compiler->enclosing == NULL)
        return -1;

    int enclosingSlot = getVarSlot(&compiler->enclosing->locals, name);
    if(enclosingSlot == -1) {
        int slot = resolveUpvalue(engine, compiler->enclosing, name);
        if(slot == -1)
            return -1;
        for(int i = 0; i < compiler->upvals.count; i++)
            if(compiler->upvals.values[i].slot == slot && !compiler->upvals.values[i].local)
                return i;
        int result = compiler->upvals.count;
        struct piccolo_Upvalue upval;
        upval.slot = slot;
        upval.local = false;
        piccolo_writeUpvalueArray(engine, &compiler->upvals, upval);
        return result;
    }

    for(int i = 0; i < compiler->upvals.count; i++)
        if(compiler->upvals.values[i].slot == enclosingSlot && compiler->upvals.values[i].local)
            return i;

    int slot = compiler->upvals.count;

    struct piccolo_Upvalue upval;
    upval.slot = enclosingSlot;
    upval.local = true;
    piccolo_writeUpvalueArray(engine, &compiler->upvals, upval);

    return slot;
}

#define COMPILE_PARAMETERS struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Bytecode* bytecode, bool requireValue, bool global
#define COMPILE_ARGUMENTS engine, compiler, bytecode, requireValue, global
#define COMPILE_ARGUMENTS_REQ_VAL engine, compiler, bytecode, true, global
#define COMPILE_ARGUMENTS_NO_VAL engine, compiler, bytecode, false, global

static void compileExpr(COMPILE_PARAMETERS);

static void compileLiteral(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_NUM) {
        piccolo_writeConst(engine, bytecode, PICCOLO_NUM_VAL(strtod(compiler->current.start, NULL)), compiler->current.charIdx);
        advanceCompiler(engine, compiler);
        return;
    }
    if(compiler->current.type == PICCOLO_TOKEN_STRING) {
        piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(piccolo_copyString(engine, compiler->current.start + 1, compiler->current.length - 2)), compiler->current.charIdx);
        advanceCompiler(engine, compiler);
        return;
    }
    if(compiler->current.type == PICCOLO_TOKEN_NIL) {
        piccolo_writeConst(engine, bytecode, PICCOLO_NIL_VAL(), compiler->current.charIdx);
        advanceCompiler(engine, compiler);
        return;
    }
    if(compiler->current.type == PICCOLO_TOKEN_FALSE) {
        piccolo_writeConst(engine, bytecode, PICCOLO_BOOL_VAL(false), compiler->current.charIdx);
        advanceCompiler(engine, compiler);
        return;
    }
    if(compiler->current.type == PICCOLO_TOKEN_TRUE) {
        piccolo_writeConst(engine, bytecode, PICCOLO_BOOL_VAL(true), compiler->current.charIdx);
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

static void compileRange(COMPILE_PARAMETERS) {
    compileLiteral(COMPILE_ARGUMENTS);
    if(compiler->current.type == PICCOLO_TOKEN_DOT_DOT) {
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileLiteral(COMPILE_ARGUMENTS);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_CREATE_RANGE, charIdx);
    }
}

static void compileImport(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_IMPORT) {
        advanceCompiler(engine, compiler);
        int charIdx = compiler->current.charIdx;
        if(compiler->current.type != PICCOLO_TOKEN_STRING) {
            compilationError(engine, compiler, "Expected package name.");
            advanceCompiler(engine, compiler);
            return;
        }
        int packageSlot = -1;
        for(int i = 0; i < engine->packages.count; i++) {
            if(strlen(engine->packages.values[i]->packageName) == compiler->current.length - 2) {
                if(memcmp(engine->packages.values[i]->packageName, compiler->current.start + 1, compiler->current.length - 2) == 0) {
                    packageSlot = i;
                    break;
                }
            }
        }
        if(packageSlot == -1) {
            compilationError(engine, compiler, "Cannot find package %.*s.", compiler->current.length, compiler->current.start);
        } else {
            piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(engine->packages.values[packageSlot]), charIdx);
        }
        advanceCompiler(engine, compiler);
        return;
    }
    compileRange(COMPILE_ARGUMENTS);
}

static void compileFnLiteral(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_FN) {
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);

        struct piccolo_Compiler functionCompiler;
        initCompiler(engine, &functionCompiler, compiler->scanner->source, true);
        functionCompiler.scanner = compiler->scanner;
        functionCompiler.globals = compiler->globals;

        struct piccolo_ObjFunction* function = piccolo_newFunction(engine);

        while(compiler->current.type != PICCOLO_TOKEN_ARROW) {
            if(compiler->current.type == PICCOLO_TOKEN_EOF) {
                compilationError(engine, compiler, "Expected ->.");
                return;
            }
            struct piccolo_Token parameterName = compiler->current;
            if(compiler->current.type != PICCOLO_TOKEN_IDENTIFIER) {
                compilationError(engine, compiler, "Expected parameter name.");
                break;
            } else {
                struct piccolo_Variable parameter;
                parameter.name = parameterName.start;
                parameter.nameLen = parameterName.length;
                parameter.slot = functionCompiler.locals.count;
                parameter.nameInSource = true;
                piccolo_writeVariableArray(engine, &functionCompiler.locals, parameter);
                function->arity++;
                advanceCompiler(engine, compiler);
                if(compiler->current.type == PICCOLO_TOKEN_COMMA) {
                    advanceCompiler(engine, compiler);
                    if(compiler->current.type != PICCOLO_TOKEN_IDENTIFIER) {
                        compilationError(engine, compiler, "Expected parameter name.");
                        break;
                    }
                } else if(compiler->current.type != PICCOLO_TOKEN_ARROW) {
                    compilationError(engine, compiler, "Expected comma.");
                    break;
                }
            }
        }

        advanceCompiler(engine, compiler);
        functionCompiler.current = compiler->current;
        functionCompiler.cycled = false;
        functionCompiler.hadError = false;
        functionCompiler.enclosing = compiler;

        compileExpr(engine, &functionCompiler, &function->bytecode, true, false);
        piccolo_writeBytecode(engine, &function->bytecode, PICCOLO_OP_RETURN, 1);

        if(functionCompiler.hadError)
            compiler->hadError = true;

        compiler->current = functionCompiler.current;

        piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(function), charIdx);
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CLOSURE, functionCompiler.upvals.count, charIdx);
        for(int i = 0; i < functionCompiler.upvals.count; i++) {
            int slot = functionCompiler.upvals.values[i].slot;
            piccolo_writeBytecode(engine, bytecode, (slot & 0xFF00) >> 8, charIdx);
            piccolo_writeBytecode(engine, bytecode, (slot & 0x00FF) >> 0, charIdx);
            piccolo_writeBytecode(engine, bytecode, functionCompiler.upvals.values[i].local, charIdx);
        }
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_CLOSE_UPVALS, charIdx);

        freeCompiler(engine, &functionCompiler);
        return;
    }
    compileImport(COMPILE_ARGUMENTS);
}

static void compileVarLookup(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_IDENTIFIER) {
        struct piccolo_Token varName = compiler->current;
        int globalSlot = getVarSlot(compiler->globals, varName);
        if(globalSlot == -1) {
            int localSlot = getVarSlot(&compiler->locals, varName);
            if(localSlot == -1) {
                int upvalSlot = resolveUpvalue(engine, compiler, varName);
                if(upvalSlot == -1) {
                    compilationError(engine, compiler, "Variable %.*s is not defined.", varName.length, varName.start);
                } else {
                    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_GET_UPVAL, upvalSlot, varName.charIdx);
                }
            } else {
                piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_GET_LOCAL, localSlot, varName.charIdx);
            }
        } else {
            piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_GET_GLOBAL, globalSlot, varName.charIdx);
        }
        advanceCompiler(engine, compiler);
        return;
    }
    compileFnLiteral(COMPILE_ARGUMENTS);
}

static void compileArray(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_LEFT_SQR_PAREN) {
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        int elementCount = 0;
        while(compiler->current.type != PICCOLO_TOKEN_RIGHT_SQR_PAREN) {
            compileExpr(COMPILE_ARGUMENTS_REQ_VAL);
            elementCount++;
            if(compiler->current.type == PICCOLO_TOKEN_EOF) {
                compilationError(engine, compiler, "Expected comma.");
                break;
            }
            if(compiler->current.type != PICCOLO_TOKEN_COMMA && compiler->current.type != PICCOLO_TOKEN_RIGHT_SQR_PAREN) {
                compilationError(engine, compiler, "Expected comma.");
                advanceCompiler(engine, compiler);
            } else if(compiler->current.type == PICCOLO_TOKEN_COMMA) {
                advanceCompiler(engine, compiler);
            }
        }
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CREATE_ARRAY, elementCount, charIdx);
        advanceCompiler(engine, compiler);
        return;
    }
    compileVarLookup(COMPILE_ARGUMENTS);
}

static void compileIndexing(COMPILE_PARAMETERS) {
    compileArray(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_LEFT_SQR_PAREN) {
        struct piccolo_Scanner scannerCopy = *compiler->scanner;
        struct piccolo_Token startTokenCopy = compiler->current;
        advanceCompiler(engine, compiler);
        int charIdx = compiler->current.charIdx;
        int startBytecodeCount = bytecode->code.count;
        compileExpr(COMPILE_ARGUMENTS_REQ_VAL);
        if(compiler->current.type == PICCOLO_TOKEN_RIGHT_SQR_PAREN) {
            advanceCompiler(engine, compiler);
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GET_IDX, charIdx);
        } else if(compiler->current.type == PICCOLO_TOKEN_COMMA) {
            // This cursed code that breaks the "single pass"-ness of the compiler is needed for cases like
            // var x = [0, 1, 2, 3, 4][0]
            // [1, 2, 3]
            *compiler->scanner = scannerCopy;
            compiler->current = startTokenCopy;
            bytecode->code.count = startBytecodeCount;
            bytecode->charIdxs.count = startBytecodeCount;
            return;
        } else {
            compilationError(engine, compiler, "Expected ].");
        }
    }
}

static void compileFnCall(COMPILE_PARAMETERS) {
    compileIndexing(COMPILE_ARGUMENTS);
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
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CALL, args, charIdx);
    }
}

static void compileUnary(COMPILE_PARAMETERS) {
    while(compiler->current.type == PICCOLO_TOKEN_MINUS || compiler->current.type == PICCOLO_TOKEN_BANG) {
        enum piccolo_TokenType op = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        if(op == PICCOLO_TOKEN_MINUS)
            piccolo_writeConst(engine, bytecode, PICCOLO_NUM_VAL(0), charIdx);
        compileFnCall(COMPILE_ARGUMENTS_REQ_VAL);
        if(op == PICCOLO_TOKEN_MINUS)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SUB, charIdx);
        if(op == PICCOLO_TOKEN_BANG)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_NOT, charIdx);
        return;
    }
    compileFnCall(COMPILE_ARGUMENTS);
}

static void compileMultiplicative(COMPILE_PARAMETERS) {
    compileUnary(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_STAR || compiler->current.type == PICCOLO_TOKEN_SLASH || compiler->current.type == PICCOLO_TOKEN_PERCENT) {
        enum piccolo_TokenType operation = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileUnary(COMPILE_ARGUMENTS_REQ_VAL);
        if(operation == PICCOLO_TOKEN_STAR)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_MUL, charIdx);
        if(operation == PICCOLO_TOKEN_SLASH)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_DIV, charIdx);
        if(operation == PICCOLO_TOKEN_PERCENT)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_MOD, charIdx);
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
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_ADD, charIdx);
        if(operation == PICCOLO_TOKEN_MINUS)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SUB, charIdx);
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
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GREATER, charIdx);
        if(operation == PICCOLO_TOKEN_LESS)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_LESS, charIdx);
        if(operation == PICCOLO_TOKEN_GREATER_EQ) {
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_LESS, charIdx);
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_NOT, charIdx);
        }
        if(operation == PICCOLO_TOKEN_LESS_EQ) {
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GREATER, charIdx);
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_NOT, charIdx);
        }
    }
}

static void compileEquality(COMPILE_PARAMETERS) {
    compileComparison(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_EQ_EQ) {
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        compileComparison(COMPILE_ARGUMENTS_REQ_VAL);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_EQUAL, charIdx);
    }
}

static void compileBoolOperations(COMPILE_PARAMETERS) {
    compileEquality(COMPILE_ARGUMENTS);
    while(compiler->current.type == PICCOLO_TOKEN_AND || compiler->current.type == PICCOLO_TOKEN_OR) {
        enum piccolo_TokenType op = compiler->current.type;
        int charIdx = compiler->current.charIdx;
        advanceCompiler(engine, compiler);
        if(op == PICCOLO_TOKEN_OR) {
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_NOT, charIdx);
        }
        int shortCircuitJumpAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP_FALSE, 0, charIdx);
        compileEquality(COMPILE_ARGUMENTS_REQ_VAL);
        int skipValueJumpAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP, 0, charIdx);
        int valueAddr = bytecode->code.count;
        piccolo_writeConst(engine, bytecode, PICCOLO_BOOL_VAL(op == PICCOLO_TOKEN_OR), charIdx);
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
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SET, charIdx);
        if(!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);
    } else {
        if(!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);
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
                    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_GET_GLOBAL, slot, varName.charIdx);
                }
            } else {
                slot = getVarSlot(&compiler->locals, varName);
                if(slot != -1) {
                    compilationError(engine, compiler, "Variable %.*s already defined.", varName.length, varName.start);
                } else {
                    slot = compiler->locals.count;
                    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_GET_LOCAL, slot, varName.charIdx);
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
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SET, eqCharIdx);

        if(global) {
            struct piccolo_Variable variable;
            variable.slot = slot;
            variable.name = varName.start;
            variable.nameLen = varName.length;
            variable.nameInSource = true;
            piccolo_writeVariableArray(engine, compiler->globals, variable);
        } else {
            struct piccolo_Variable variable;
            variable.slot = slot;
            variable.name = varName.start;
            variable.nameLen = varName.length;
            variable.nameInSource = true;
            piccolo_writeVariableArray(engine, &compiler->locals, variable);
        }

        if(!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, eqCharIdx);
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
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP_FALSE, 0, charIdx);
        compileExpr(COMPILE_ARGUMENTS_REQ_VAL);

        int ifEndAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP, 0, charIdx);
        int elseStartAddr = bytecode->code.count;

        if(compiler->current.type == PICCOLO_TOKEN_ELSE) {
            advanceCompiler(engine, compiler);
            compileExpr(COMPILE_ARGUMENTS_REQ_VAL);
        } else {
            piccolo_writeConst(engine, bytecode, PICCOLO_NIL_VAL(), charIdx);
        }

        int elseEndAddr = bytecode->code.count;

        piccolo_patchParam(bytecode, ifStartAddr, elseStartAddr - ifStartAddr);
        piccolo_patchParam(bytecode, ifEndAddr, elseEndAddr - ifEndAddr);

        if(!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);
        return;
    }
    compileVarDecl(COMPILE_ARGUMENTS);
}

static void compileWhile(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_WHILE) {
        advanceCompiler(engine, compiler);
        int charIdx = compiler->current.charIdx;

        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CREATE_ARRAY, 0, charIdx);

        int whileStartAddr = bytecode->code.count;
        compileExpr(COMPILE_ARGUMENTS_REQ_VAL);
        int fwdJumpAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP_FALSE, 0, charIdx);

        compileExpr(COMPILE_ARGUMENTS_REQ_VAL);

        if(requireValue || compiler->current.type == PICCOLO_TOKEN_RIGHT_BRACE) {
            piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CREATE_ARRAY, 1, charIdx);
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_ADD, charIdx);
        } else {
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);
        }

        int revJumpAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_REV_JUMP, 0, charIdx);
        int whileEndAddr = bytecode->code.count;

        if(!(requireValue || compiler->current.type == PICCOLO_TOKEN_RIGHT_BRACE))
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);

        piccolo_patchParam(bytecode, fwdJumpAddr, whileEndAddr - fwdJumpAddr);
        piccolo_patchParam(bytecode, revJumpAddr, revJumpAddr - whileStartAddr);

        return;
    }
    compileIf(COMPILE_ARGUMENTS);
}

static void compileFor(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_FOR) {
        advanceCompiler(engine, compiler);
        struct piccolo_Token varName = compiler->current;
        if(varName.type != PICCOLO_TOKEN_IDENTIFIER) {
            compilationError(engine, compiler, "Expected variable name.");
            return;
        }
        advanceCompiler(engine, compiler);
        if(compiler->current.type != PICCOLO_TOKEN_IN) {
            compilationError(engine, compiler, "Expected in.");
            return;
        }
        advanceCompiler(engine, compiler);

        int slot = -1;
        if(getVarSlot(&compiler->locals, varName) != -1) {
            compilationError(engine, compiler, "Variable %.*s is already defined.");
            return;
        } else {
            struct piccolo_Variable iterVar;
            iterVar.nameInSource = true;
            iterVar.name = varName.start;
            iterVar.nameLen = varName.length;
            slot = iterVar.slot = compiler->locals.count;
            piccolo_writeVariableArray(engine, &compiler->locals, iterVar);
        }

        // If value required:
        // arr idx [prev]
        // arr idx [prev] &iterVar
        // arr idx [prev] &iterVar arr
        // arr idx [prev] &iterVar arr idx
        // arr idx [prev] &iterVar &elem
        // arr idx [prev] &elem
        // arr idx [prev]
        // arr [prev] idx
        // arr [prev] idx 1
        // arr [prev] (idx + 1)
        // arr (idx + 1) [prev]
        // arr (idx + 1) [prev] loopVal
        // arr (idx + 1) [prev] [loopVal]
        // arr (idx + 1) [prev, loopVal]

        int charIdx = compiler->current.charIdx;
        compileExpr(COMPILE_ARGUMENTS_REQ_VAL); // arr
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_EVAPORATE_PTR, charIdx);
        piccolo_writeConst(engine, bytecode, PICCOLO_NUM_VAL(0), charIdx);
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CREATE_ARRAY, 0, charIdx);
        int loopStartAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_GET_LOCAL, slot, charIdx);
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_PEEK_STACK, 4, charIdx);
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_PEEK_STACK, 4, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GET_IDX, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SET, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SWAP_STACK, charIdx);
        piccolo_writeConst(engine, bytecode, PICCOLO_NUM_VAL(1), charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_ADD, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SWAP_STACK, charIdx);

        compileExpr(COMPILE_ARGUMENTS_REQ_VAL); // Loop val

        if(requireValue || compiler->current.type == PICCOLO_TOKEN_RIGHT_BRACE) {
            piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CREATE_ARRAY, 1, charIdx);
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_ADD, charIdx);
        } else {
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);
        }

        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_PEEK_STACK, 3, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GET_LEN, charIdx);
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_PEEK_STACK, 3, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GREATER, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_NOT, charIdx);
        int jumpAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_REV_JUMP_FALSE, jumpAddr - loopStartAddr, charIdx);

        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SWAP_STACK, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SWAP_STACK, charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);

        compiler->locals.count--;

        return;
    }
    compileWhile(COMPILE_ARGUMENTS);
}

static void compileBlock(COMPILE_PARAMETERS) {
    if(compiler->current.type == PICCOLO_TOKEN_LEFT_BRACE) {
        int localsBefore = compiler->locals.count;
        advanceCompiler(engine, compiler);
        if(compiler->current.type == PICCOLO_TOKEN_RIGHT_BRACE) {
            int charIdx = compiler->current.charIdx;
            piccolo_writeConst(engine, bytecode, PICCOLO_NIL_VAL(), charIdx);
            advanceCompiler(engine, compiler);
        } else {
            while (compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE) {
                if (compiler->current.type == PICCOLO_TOKEN_EOF) {
                    compilationError(engine, compiler, "Expected }.");
                    break;
                }
                compileFor(engine, compiler, bytecode, false, false);
            }
            advanceCompiler(engine, compiler);
            int charIdx = compiler->current.charIdx;
            if (!requireValue && compiler->current.type != PICCOLO_TOKEN_RIGHT_BRACE)
                piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, charIdx);
            compiler->locals.count = localsBefore;
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_CLOSE_UPVALS, charIdx);
        }
        return;
    }
    compileFor(COMPILE_ARGUMENTS);
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

bool piccolo_compilePackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    struct piccolo_Compiler compiler;
    struct piccolo_Scanner scanner;
    compiler.scanner = &scanner;
    initCompiler(engine, &compiler, package->source, false);
    compiler.globals = &package->globalVars;
    compiler.enclosing = NULL;

    compileExpr(engine, &compiler, &package->bytecode, false, true);
    if(compiler.current.type != PICCOLO_TOKEN_EOF) {
        compilationError(engine, &compiler, "Expected EOF.");
    }

    freeCompiler(engine, &compiler);

    piccolo_writeBytecode(engine, &package->bytecode, PICCOLO_OP_RETURN, 1);
    return !compiler.hadError;
}

#undef COMPILE_PARAMETERS
#undef COMPILE_ARGUMENTS
#undef COMPILE_ARGUMENTS_REQ_VAL
#undef COMPILE_ARGUMENTS_NO_VAL
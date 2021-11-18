
#include "compiler.h"
#include "package.h"
#include "parser.h"
#include "bytecode.h"

#include "debug/expr.h"

#include <stdlib.h>
#include <string.h>

#include "debug/disassembler.h"
#include "util/strutil.h"

PICCOLO_DYNARRAY_IMPL(struct piccolo_Variable, Variable)

// Some compiler utility functions

static void initCompiler(struct piccolo_Compiler* compiler, struct piccolo_Package* package, struct piccolo_VariableArray* globals) {
    compiler->hadError = false;
    compiler->package = package;
    compiler->globals = globals;
    piccolo_initVariableArray(&compiler->locals);
}

static void compilationError(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, int charIdx, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
    struct piccolo_strutil_LineInfo line = piccolo_strutil_getLine(compiler->package->source, charIdx);
    piccolo_enginePrintError(engine, "\n[line %d] %.*s\n", line.line + 1, line.lineEnd - line.lineStart, line.lineStart);

    int lineNumberDigits = 0;
    int lineNumber = line.line + 1;
    while(lineNumber > 0) {
        lineNumberDigits++;
        lineNumber /= 10;
    }
    piccolo_enginePrintError(engine, "%*c ^", 7 + lineNumberDigits + charIdx - (line.lineStart - compiler->package->source), ' ');
    piccolo_enginePrintError(engine, "\n");

    compiler->hadError = true;
}

static int getGlobalSlot(struct piccolo_Compiler* compiler, struct piccolo_Token token) {
    for(int i = 0; i < compiler->globals->count; i++) {
        struct piccolo_Variable var = compiler->globals->values[i];
        if(var.nameLen == token.length && memcmp(var.nameStart, token.start, var.nameLen) == 0)
            return var.slot;
    }
    return -1;
}

static int getLocalSlot(struct piccolo_Compiler* compiler, struct piccolo_Token token) {
    for(int i = 0; i < compiler->locals.count; i++) {
        struct piccolo_Variable var = compiler->locals.values[i];
        if(var.nameLen == token.length && memcmp(var.nameStart, token.start, var.nameLen) == 0)
            return var.slot;
    }
    return -1;
}

static struct piccolo_Variable createVar(struct piccolo_Token name, int slot) {
    struct piccolo_Variable var;
    var.nameStart = name.start;
    var.nameLen = name.length;
    var.slot = slot;
    var.mutable = true;
    return var;
}

static struct piccolo_Package* resolvePackage(struct piccolo_Engine* engine, const char* name, size_t nameLen) {
    for(int i = 0; i < engine->packages.count; i++) {
        if(memcmp(engine->packages.values[i]->packageName, name, nameLen) == 0) {
            return engine->packages.values[i];
        }
    }
    return NULL;
}

struct variableData {
    int slot;
    enum piccolo_OpCode setOp;
    enum piccolo_OpCode getOp;
    bool global;
};

static struct variableData getVariable(struct piccolo_Compiler* compiler, struct piccolo_Token name) {
    int globalSlot = getGlobalSlot(compiler, name);

    struct variableData result;
    if(globalSlot == -1) {
        int localSlot = getLocalSlot(compiler, name);
        if(localSlot == -1) {
            result.slot = -1;
        } else {
            result.slot = localSlot;
            result.getOp = PICCOLO_OP_GET_LOCAL;
            result.setOp = PICCOLO_OP_SET_LOCAL;
            result.global = false;
        }
    } else {
        result.slot = globalSlot;
        result.getOp = PICCOLO_OP_GET_GLOBAL;
        result.setOp = PICCOLO_OP_SET_GLOBAL;
        result.global = true;
    }

    return result;
}

/*
    Recursively determines which expressions values need to be calculated.
    It's important to evaluate as little as possible, because evaluating things like loops
    can be very costly x_x
 */
static void markReqEval(struct piccolo_ExprNode* expr) {
    switch(expr->type) {
        case PICCOLO_EXPR_SUBSCRIPT: {
            struct piccolo_SubscriptNode* subscript = (struct piccolo_SubscriptNode*)expr;
            subscript->value->reqEval = true;
            markReqEval(subscript->value);
            break;
        }
        case PICCOLO_EXPR_UNARY: {
            struct piccolo_UnaryNode* unary = (struct piccolo_UnaryNode*)expr;
            unary->value->reqEval = expr->reqEval;
            markReqEval(unary->value);
            break;
        }
        case PICCOLO_EXPR_BINARY: {
            struct piccolo_BinaryNode* binary = (struct piccolo_BinaryNode*)expr;
            binary->a->reqEval = expr->reqEval;
            markReqEval(binary->a);
            binary->b->reqEval = expr->reqEval;
            markReqEval(binary->b);
            break;
        }
        case PICCOLO_EXPR_BLOCK: {
            struct piccolo_BlockNode* block = (struct piccolo_BlockNode*)expr;
            struct piccolo_ExprNode* curr = block->first;
            while(curr != NULL) {
                if(curr->nextExpr == NULL)
                    curr->reqEval = expr->reqEval;
                markReqEval(curr);
                curr = curr->nextExpr;
            }
            break;
        }
        case PICCOLO_EXPR_VAR_DECL: {
            struct piccolo_VarDeclNode* varDecl = (struct piccolo_VarDeclNode*)expr;
            varDecl->value->reqEval = true;
            markReqEval(varDecl->value);
            break;
        }
        case PICCOLO_EXPR_VAR_SET: {
            struct piccolo_VarSetNode* varSet = (struct piccolo_VarSetNode*)expr;
            varSet->value->reqEval = true;
            markReqEval(varSet->value);
            break;
        }
        case PICCOLO_EXPR_SUBSCRIPT_SET: {
            struct piccolo_SubscriptSetNode* subscriptSet = (struct piccolo_SubscriptSetNode*)expr;
            subscriptSet->target->reqEval = true;
            markReqEval(subscriptSet->target);
            subscriptSet->value->reqEval = true;
            markReqEval(subscriptSet->value);
            break;
        }
        case PICCOLO_EXPR_IF: {
            struct piccolo_IfNode* ifNode = (struct piccolo_IfNode*)expr;
            ifNode->condition->reqEval = true;
            markReqEval(ifNode->condition);
            ifNode->trueVal->reqEval = ifNode->expr.reqEval;
            markReqEval(ifNode->trueVal);
            if(ifNode->falseVal != NULL) {
                ifNode->falseVal->reqEval = ifNode->expr.reqEval;
                markReqEval(ifNode->falseVal);
            }
            break;
        }
        case PICCOLO_EXPR_WHILE: {
            struct piccolo_WhileNode* whileNode = (struct piccolo_WhileNode*)expr;
            whileNode->condition->reqEval = true;
            markReqEval(whileNode->condition);
            whileNode->value->reqEval = whileNode->expr.reqEval;
            markReqEval(whileNode->value);
            break;
        }
        case PICCOLO_EXPR_CALL: {
            struct piccolo_CallNode* call = (struct piccolo_CallNode*)expr;
            call->function->reqEval = true;
            markReqEval(call->function);
            struct piccolo_ExprNode* currArg = call->firstArg;
            while(currArg != NULL) {
                currArg->reqEval = true;
                markReqEval(currArg);
                currArg = currArg->nextExpr;
            }
            break;
        }
        default: {
            break;
        }
    }
}

static void findGlobals(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_ExprNode* ast) {
    struct piccolo_ExprNode *curr = ast;
    while (curr != NULL) {
        switch (curr->type) {
            case PICCOLO_EXPR_VAR_DECL: {
                struct piccolo_VarDeclNode* varDecl = (struct piccolo_VarDeclNode*)curr;
                if(getGlobalSlot(compiler, varDecl->name) != -1) {
                    compilationError(engine, compiler, varDecl->name.charIdx, "Variable '%.*s' already defined.", varDecl->name.length, varDecl->name.start);
                } else {
                    struct piccolo_Variable var = createVar(varDecl->name, compiler->globals->count);
                    var.mutable = varDecl->mutable;
                    piccolo_writeVariableArray(engine, compiler->globals, var);
                }
                break;
            }
            default:
                break;
        }
        curr = curr->nextExpr;
    }
}

#define COMPILE_PARAMS struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, struct piccolo_Compiler* compiler, bool local
#define COMPILE_ARGS engine, bytecode, compiler, local

static void compileExpr(struct piccolo_ExprNode* expr, COMPILE_PARAMS);

static void compileLiteral(struct piccolo_LiteralNode* literal, COMPILE_PARAMS) {
    if(!literal->expr.reqEval)
        return;

    switch(literal->token.type) {
        case PICCOLO_TOKEN_NUM: {
            double value = strtod(literal->token.start, NULL);
            piccolo_writeConst(engine, bytecode, PICCOLO_NUM_VAL(value), literal->token.charIdx);
            break;
        }
        case PICCOLO_TOKEN_STRING: {
            struct piccolo_ObjString* value = piccolo_copyString(engine, literal->token.start + 1, (int)literal->token.length - 2);
            piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(value), literal->token.charIdx);
            break;
        }
        case PICCOLO_TOKEN_TRUE: {
            piccolo_writeConst(engine, bytecode, PICCOLO_BOOL_VAL(true), literal->token.charIdx);
            break;
        }
        case PICCOLO_TOKEN_FALSE: {
            piccolo_writeConst(engine, bytecode, PICCOLO_BOOL_VAL(false), literal->token.charIdx);
            break;
        }
        case PICCOLO_TOKEN_NIL: {
            piccolo_writeConst(engine, bytecode, PICCOLO_NIL_VAL(), literal->token.charIdx);
            break;
        }
        default: {

        }
    }
}

static void compileVar(struct piccolo_VarNode* var, COMPILE_PARAMS) {
    struct variableData varData = getVariable(compiler, var->name);
    if(varData.slot == -1) {
        compilationError(engine, compiler, var->name.charIdx, "Variable '%.*s' is not defined.", var->name.length, var->name.start);
        return;
    }
    if(!var->expr.reqEval)
        return;
    piccolo_writeParameteredBytecode(engine, bytecode, varData.getOp, varData.slot, var->name.charIdx);
}

static void compileSubscript(struct piccolo_SubscriptNode* subscript, COMPILE_PARAMS) {
    compileExpr(subscript->value, COMPILE_ARGS);
    if(subscript->expr.reqEval) {
        struct piccolo_ObjString* subscriptStr = piccolo_copyString(engine, subscript->subscript.start, subscript->subscript.length);
        piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(subscriptStr), subscript->subscript.charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GET_IDX, subscript->subscript.charIdx);
    }
}

static void compileUnary(struct piccolo_UnaryNode* unary, COMPILE_PARAMS) {
    compileExpr(unary->value, COMPILE_ARGS);
    switch(unary->op.type) {
        case PICCOLO_TOKEN_MINUS: {
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_NEGATE, unary->op.charIdx);
            break;
        }
        case PICCOLO_TOKEN_BANG: {
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_NOT, unary->op.charIdx);
            break;
        }
        default: {

        }
    }
}

static void compileBinary(struct piccolo_BinaryNode* binary, COMPILE_PARAMS) {
    if(binary->op.type == PICCOLO_TOKEN_PLUS ||
       binary->op.type == PICCOLO_TOKEN_MINUS ||
       binary->op.type == PICCOLO_TOKEN_STAR ||
       binary->op.type == PICCOLO_TOKEN_SLASH ||
       binary->op.type == PICCOLO_TOKEN_PERCENT ||
       binary->op.type == PICCOLO_TOKEN_EQ_EQ ||
       binary->op.type == PICCOLO_TOKEN_BANG_EQ ||
       binary->op.type == PICCOLO_TOKEN_GREATER ||
       binary->op.type == PICCOLO_TOKEN_LESS ||
       binary->op.type == PICCOLO_TOKEN_GREATER_EQ ||
       binary->op.type == PICCOLO_TOKEN_LESS_EQ) {
        compileExpr(binary->a, COMPILE_ARGS);
        compileExpr(binary->b, COMPILE_ARGS);

        if(!binary->expr.reqEval)
            return;

        enum piccolo_OpCode op = 0;
        switch(binary->op.type) {
            case PICCOLO_TOKEN_PLUS: {
                op = PICCOLO_OP_ADD;
                break;
            }
            case PICCOLO_TOKEN_MINUS: {
                op = PICCOLO_OP_SUB;
                break;
            }
            case PICCOLO_TOKEN_STAR: {
                op = PICCOLO_OP_MUL;
                break;
            }
            case PICCOLO_TOKEN_SLASH: {
                op = PICCOLO_OP_DIV;
                break;
            }
            case PICCOLO_TOKEN_PERCENT: {
                op = PICCOLO_OP_MOD;
                break;
            }
            case PICCOLO_TOKEN_EQ_EQ:
            case PICCOLO_TOKEN_BANG_EQ: {
                op = PICCOLO_OP_EQUAL;
                break;
            }
            case PICCOLO_TOKEN_GREATER:
            case PICCOLO_TOKEN_LESS_EQ: {
                op = PICCOLO_OP_GREATER;
                break;
            }
            case PICCOLO_TOKEN_LESS:
            case PICCOLO_TOKEN_GREATER_EQ: {
                op = PICCOLO_OP_LESS;
                break;
            }
            default: {
                break;
            }
        }

        piccolo_writeBytecode(engine, bytecode, op, binary->op.charIdx);
        if(binary->op.type == PICCOLO_TOKEN_BANG_EQ ||
           binary->op.type == PICCOLO_TOKEN_GREATER_EQ ||
           binary->op.type == PICCOLO_TOKEN_LESS_EQ) {
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_NOT, binary->op.charIdx);
        }
    }
}

static void compileBlock(struct piccolo_BlockNode* block, COMPILE_PARAMS) {
    if(block->first == NULL) {
        if(block->expr.reqEval)
            piccolo_writeConst(engine, bytecode, PICCOLO_NIL_VAL(), 0);
    } else {
        struct piccolo_ExprNode *curr = block->first;
        int localCount = compiler->locals.count;
        while (curr != NULL) {
            compileExpr(curr, engine, bytecode, compiler, true);
            curr = curr->nextExpr;
        }
        compiler->locals.count = localCount;
    }
}

static void compileVarDecl(struct piccolo_VarDeclNode* varDecl, COMPILE_PARAMS) {
    compileExpr(varDecl->value, COMPILE_ARGS);
    if(local) { // Act locally
        struct variableData varData = getVariable(compiler, varDecl->name);
        if(varData.slot != -1) {
            compilationError(engine, compiler, varDecl->name.charIdx, "Variable '%.*s' already defined.", varDecl->name.length, varDecl->name.start);
        } else {
            int slot = compiler->locals.count;
            struct piccolo_Variable var = createVar(varDecl->name, slot);
            var.mutable = varDecl->mutable;
            piccolo_writeVariableArray(engine, &compiler->locals, var);
            piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_SET_LOCAL, slot, varDecl->name.charIdx);
        }
    } else { // Think globally
        int slot = getGlobalSlot(compiler, varDecl->name);
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_SET_GLOBAL, slot, varDecl->name.charIdx);
    }
    if(!varDecl->expr.reqEval)
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, varDecl->name.charIdx);
}

static void compileVarSet(struct piccolo_VarSetNode* varSet, COMPILE_PARAMS) {
    struct variableData varData = getVariable(compiler, varSet->name);
    compileExpr(varSet->value, COMPILE_ARGS);
    if(varData.slot == -1) {
        compilationError(engine, compiler, varSet->name.charIdx, "Variable '%.*s' is not defined.", varSet->name.length, varSet->name.start);
    } else {
        struct piccolo_Variable var = varData.global ? compiler->globals->values[varData.slot] : compiler->locals.values[varData.slot];
        if(!var.mutable) {
            compilationError(engine, compiler, varSet->name.charIdx, "Cannot modify immutable variable.");
            return;
        }
        piccolo_writeParameteredBytecode(engine, bytecode, varData.setOp, varData.slot, varSet->name.charIdx);
    }
    if(!varSet->expr.reqEval)
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, varSet->name.charIdx);
}

static void compileSubscriptSet(struct piccolo_SubscriptSetNode* subscriptSet, COMPILE_PARAMS) {
    compileExpr(subscriptSet->target, COMPILE_ARGS);

    struct piccolo_ObjString* subscriptStr = piccolo_copyString(engine, subscriptSet->subscript.start, subscriptSet->subscript.length);
    piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(subscriptStr), subscriptSet->subscript.charIdx);

    compileExpr(subscriptSet->value, COMPILE_ARGS);

    piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SET_IDX, subscriptSet->subscript.charIdx);
    if(!subscriptSet->expr.reqEval)
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, subscriptSet->subscript.charIdx);
}

/*
    If only true expression:

    - condition -
    jump if false to end
    - trueExpr -
    end:

    If true and false expressions:

    - condition -
    jump if false to skipTrue
    - trueExpr -
    jump to end
    skipTrue:
    - falseExpr -
    end:
*/
static void compileIf(struct piccolo_IfNode* ifNode, COMPILE_PARAMS) {
    compileExpr(ifNode->condition, COMPILE_ARGS);

    int skipTrueAddr = bytecode->code.count;
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP_FALSE, 0, ifNode->conditionCharIdx);
    compileExpr(ifNode->trueVal, COMPILE_ARGS);

    int skipFalseAddr = bytecode->code.count;
    if(ifNode->falseVal != NULL)
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP, 0, ifNode->conditionCharIdx);

    int falseStartAddr = bytecode->code.count;
    piccolo_patchParam(bytecode, skipTrueAddr, falseStartAddr - skipTrueAddr);

    if(ifNode->falseVal != NULL) {
        compileExpr(ifNode->falseVal, COMPILE_ARGS);
        int endAddr = bytecode->code.count;
        piccolo_patchParam(bytecode, skipFalseAddr, endAddr - skipFalseAddr);
    }
}

static void compileWhile(struct piccolo_WhileNode* whileNode, COMPILE_PARAMS) {
    if(whileNode->expr.reqEval)
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CREATE_ARRAY, 0, 0);
    int loopStartAddr = bytecode->code.count;
    compileExpr(whileNode->condition, COMPILE_ARGS);
    int skipLoopAddr = bytecode->code.count;
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP_FALSE, 0, whileNode->conditionCharIdx);
    compileExpr(whileNode->value, COMPILE_ARGS);
    if(whileNode->expr.reqEval)
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_APPEND, 0);
    int valueEndAddr = bytecode->code.count;
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_REV_JUMP, valueEndAddr - loopStartAddr, whileNode->conditionCharIdx);
    int loopEndAddr = bytecode->code.count;
    piccolo_patchParam(bytecode, skipLoopAddr, loopEndAddr - skipLoopAddr);
}

static void compileCall(struct piccolo_CallNode* call, COMPILE_PARAMS) {
    compileExpr(call->function, COMPILE_ARGS);
    int argCount = 0;
    struct piccolo_ExprNode* currArg = call->firstArg;
    while(currArg != NULL) {
        compileExpr(currArg, COMPILE_ARGS);
        argCount++;
        currArg = currArg->nextExpr;
    }
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CALL, argCount, call->charIdx);
    if(!call->expr.reqEval)
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, call->charIdx);
}

static void compileImport(struct piccolo_ImportNode* import, COMPILE_PARAMS) {
    const char* packageName = import->packageName.start + 1;
    size_t packageLen = import->packageName.length - 2;
    struct piccolo_Package *package = resolvePackage(engine, packageName, packageLen);
    if(package == NULL) {
        compilationError(engine, compiler, import->packageName.charIdx, "Package %.*s does not exist.", import->packageName.length, import->packageName.start);
    } else {
        piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(package), import->packageName.charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_EXECUTE_PACKAGE, import->packageName.charIdx);
        if(!import->expr.reqEval)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, import->packageName.charIdx);
    }
}

static void compileExpr(struct piccolo_ExprNode* expr, COMPILE_PARAMS) {
    switch(expr->type) {
        case PICCOLO_EXPR_LITERAL: {
            compileLiteral((struct piccolo_LiteralNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_VAR: {
            compileVar((struct piccolo_VarNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_SUBSCRIPT: {
            compileSubscript((struct piccolo_SubscriptNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_UNARY: {
            compileUnary((struct piccolo_UnaryNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_BINARY: {
            compileBinary((struct piccolo_BinaryNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_BLOCK: {
            compileBlock((struct piccolo_BlockNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_VAR_DECL: {
            compileVarDecl((struct piccolo_VarDeclNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_VAR_SET: {
            compileVarSet((struct piccolo_VarSetNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_SUBSCRIPT_SET: {
            compileSubscriptSet((struct piccolo_SubscriptSetNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_IF: {
            compileIf((struct piccolo_IfNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_WHILE: {
            compileWhile((struct piccolo_WhileNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_CALL: {
            compileCall((struct piccolo_CallNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_IMPORT: {
            compileImport((struct piccolo_ImportNode*)expr, COMPILE_ARGS);
            break;
        }
    }
}

bool piccolo_compilePackage(struct piccolo_Engine* engine, struct piccolo_Package* package) {
    struct piccolo_Scanner scanner;
    piccolo_initScanner(&scanner, package->source);

    struct piccolo_Parser parser;
    piccolo_initParser(engine, &parser, &scanner);
    struct piccolo_ExprNode* ast = piccolo_parse(engine, &parser);

    if(parser.hadError) {
        piccolo_freeParser(&parser);
        return false;
    }

    struct piccolo_ExprNode* currExpr = ast;
    while(currExpr != NULL) {
        markReqEval(currExpr);
        currExpr = currExpr->nextExpr;
    }

    struct piccolo_VariableArray globals;
    piccolo_initVariableArray(&globals);

    struct piccolo_Compiler compiler;
    initCompiler(&compiler, package, &globals);

    findGlobals(engine, &compiler, ast);

    currExpr = ast;
    while(currExpr != NULL) {
        compileExpr(currExpr, engine, &package->bytecode, &compiler, false);
        currExpr = currExpr->nextExpr;
    }

    piccolo_printExpr(ast, 0);
    piccolo_freeParser(&parser);

    piccolo_writeBytecode(engine, &package->bytecode, PICCOLO_OP_RETURN, 0);
    piccolo_disassembleBytecode(&package->bytecode);

    return !compiler.hadError;
}

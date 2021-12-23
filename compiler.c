
#include "compiler.h"
#include "package.h"
#include "parser.h"
#include "bytecode.h"

#include "debug/expr.h"

#include <stdlib.h>
#include <string.h>

#include "debug/disassembler.h"
#include "util/strutil.h"

#include "util/file.h"

#include <stdio.h>

PICCOLO_DYNARRAY_IMPL(struct piccolo_Upvalue, Upvalue)
PICCOLO_DYNARRAY_IMPL(struct piccolo_Variable, Variable)

// Some compiler utility functions

static void initCompiler(struct piccolo_Compiler* compiler, struct piccolo_Package* package, struct piccolo_VariableArray* globals) {
    compiler->hadError = false;
    compiler->package = package;
    compiler->globals = globals;
    compiler->enclosing = NULL;
    piccolo_initVariableArray(&compiler->locals);
    piccolo_initUpvalueArray(&compiler->upvals);
}

static void compilationError(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, int charIdx, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
    piccolo_enginePrintError(engine, " [%s]", compiler->package->packageName);
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

static int resolveUpvalue(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Token name) {
    if(compiler->enclosing == NULL)
        return -1;

    int enclosingSlot = getLocalSlot(compiler->enclosing, name);
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
        upval.Mutable = compiler->enclosing->upvals.values[slot].Mutable;
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
    upval.Mutable = compiler->enclosing->locals.values[enclosingSlot].Mutable;
    piccolo_writeUpvalueArray(engine, &compiler->upvals, upval);

    return slot;
}

static struct piccolo_Variable createVar(struct piccolo_Token name, int slot) {
    struct piccolo_Variable var;
    var.nameStart = name.start;
    var.nameLen = name.length;
    var.slot = slot;
    var.Mutable = true;
    return var;
}

static struct piccolo_Package* resolvePackage(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, const char* sourceFilepath, const char* name, size_t nameLen) {
    for(int i = 0; i < engine->packages.count; i++) {
        if(memcmp(engine->packages.values[i]->packageName, name, nameLen) == 0) {
            return engine->packages.values[i];
        }
    }

    char path[PICCOLO_MAX_PACKAGE + 1 /* Terminator */] = { 0 };
    piccolo_applyRelativePathToFilePath(path, name, nameLen, sourceFilepath);
    for(int i = 0; i < engine->packages.count; i++) {
        if(strcmp(path, engine->packages.values[i]->packageName) == 0) {
            return engine->packages.values[i];
        }
    }

    // readFile returns a heap buffer - this shouldn't be marked const
    char* source = piccolo_readFile(path);
    if(source == NULL) {
        for(int i = 0; i < engine->searchPaths.count; i++) {
            piccolo_applyRelativePathToFilePath(path, name, nameLen, engine->searchPaths.values[i]);
            for(int j = 0; j < engine->packages.count; j++) {
                if(strcmp(path, engine->packages.values[j]->packageName) == 0) {
                    return engine->packages.values[j];
                }
            }

            source = piccolo_readFile(path);
        }
    }
    if(source == NULL) {
        piccolo_enginePrintError(engine, "Failed to load package '%s'\n", name);
        return NULL;
    }

    struct piccolo_Package* package = piccolo_createPackage(engine);
    int pathLen = (int)strlen(path);
    char* heapPackageName = malloc(pathLen + 1);
    memcpy(heapPackageName, path, pathLen);
    heapPackageName[pathLen] = '\0';
    package->packageName = heapPackageName;
    package->source = source;
    compiler->hadError |= !piccolo_compilePackage(engine, package);

    return package;
}

struct variableData {
    int slot;
    enum piccolo_OpCode setOp;
    enum piccolo_OpCode getOp;
    bool mutable;
};

static struct variableData getVariable(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Token name) {
    int globalSlot = getGlobalSlot(compiler, name);

    struct variableData result;
    if(globalSlot == -1) {
        int localSlot = getLocalSlot(compiler, name);
        if(localSlot == -1) {
            int upvalueSlot = resolveUpvalue(engine, compiler, name);
            if(upvalueSlot == -1) {
                result.slot = -1;
            } else {
                result.slot = upvalueSlot;
                result.setOp = PICCOLO_OP_SET_UPVAL;
                result.getOp = PICCOLO_OP_GET_UPVAL;
                result.mutable = compiler->upvals.values[upvalueSlot].Mutable;
            }
        } else {
            result.slot = localSlot;
            result.getOp = PICCOLO_OP_GET_LOCAL;
            result.setOp = PICCOLO_OP_SET_LOCAL;
            result.mutable = compiler->locals.values[localSlot].Mutable;
        }
    } else {
        result.slot = globalSlot;
        result.getOp = PICCOLO_OP_GET_GLOBAL;
        result.setOp = PICCOLO_OP_SET_GLOBAL;
        result.mutable = compiler->globals->values[globalSlot].Mutable;
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
        case PICCOLO_EXPR_RANGE: {
            struct piccolo_RangeNode* range = (struct piccolo_RangeNode*)expr;
            range->left->reqEval = expr->reqEval;
            markReqEval(range->left);
            range->right->reqEval = expr->reqEval;
            markReqEval(range->right);
            break;
        }
        case PICCOLO_EXPR_ARRAY_LITERAL: {
            struct piccolo_ArrayLiteralNode* arrayLiteral = (struct piccolo_ArrayLiteralNode*)expr;
            struct piccolo_ExprNode* curr = arrayLiteral->first;
            while(curr != NULL) {
                curr->reqEval = expr->reqEval;
                markReqEval(curr);
                curr = curr->nextExpr;
            }
            break;
        }
        case PICCOLO_EXPR_HASHMAP_LITERAL: {
            struct piccolo_HashmapLiteralNode* hashmap = (struct piccolo_HashmapLiteralNode*)expr;
            struct piccolo_HashmapEntryNode* curr = hashmap->first;
            while(curr != NULL) {
                curr->key->reqEval = expr->reqEval;
                markReqEval(curr->key);
                curr->value->reqEval = expr->reqEval;
                markReqEval(curr->value);
                curr = (struct piccolo_HashmapEntryNode*)curr->expr.nextExpr;
            }
            break;
        }
        case PICCOLO_EXPR_SUBSCRIPT: {
            struct piccolo_SubscriptNode* subscript = (struct piccolo_SubscriptNode*)expr;
            subscript->value->reqEval = true;
            markReqEval(subscript->value);
            break;
        }
        case PICCOLO_EXPR_INDEX: {
            struct piccolo_IndexNode* index = (struct piccolo_IndexNode*)expr;
            index->target->reqEval = expr->reqEval;
            markReqEval(index->target);
            index->index->reqEval = expr->reqEval;
            markReqEval(index->index);
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
        case PICCOLO_EXPR_FN_LITERAL: {
            struct piccolo_FnLiteralNode* fnLiteral = (struct piccolo_FnLiteralNode*)expr;
            fnLiteral->value->reqEval = true;
            markReqEval(fnLiteral->value);
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
        case PICCOLO_EXPR_INDEX_SET: {
            struct piccolo_IndexSetNode* indexSet = (struct piccolo_IndexSetNode*)expr;
            indexSet->target->reqEval = true;
            markReqEval(indexSet->target);
            indexSet->index->reqEval = true;
            markReqEval(indexSet->index);
            indexSet->value->reqEval = true;
            markReqEval(indexSet->value);
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
        case PICCOLO_EXPR_FOR: {
            struct piccolo_ForNode* forNode = (struct piccolo_ForNode*)expr;
            forNode->container->reqEval = true;
            markReqEval(forNode->container);
            forNode->value->reqEval = expr->reqEval;
            markReqEval(forNode->value);
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
        default:
            break;
    }
}

static void findGlobals(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_ExprNode* ast) {
    struct piccolo_ExprNode *curr = ast;
    while(curr != NULL) {
        switch(curr->type) {
            case PICCOLO_EXPR_BINARY: {
                struct piccolo_BinaryNode* binary = (struct piccolo_BinaryNode*)curr;
                findGlobals(engine, compiler, binary->a);
                findGlobals(engine, compiler, binary->b);
                break;
            }
            case PICCOLO_EXPR_ARRAY_LITERAL: {
                struct piccolo_ArrayLiteralNode* arrayLiteral = (struct piccolo_ArrayLiteralNode*)curr;
                findGlobals(engine, compiler, arrayLiteral->first);
                break;
            }
            case PICCOLO_EXPR_HASHMAP_ENTRY: {
                struct piccolo_HashmapEntryNode* hashmapEntry = (struct piccolo_HashmapEntryNode*)curr;
                findGlobals(engine, compiler, hashmapEntry->key);
                findGlobals(engine, compiler, hashmapEntry->value);
                break;
            }
            case PICCOLO_EXPR_HASHMAP_LITERAL: {
                struct piccolo_HashmapLiteralNode* hashmap = (struct piccolo_HashmapLiteralNode*)curr;
                findGlobals(engine, compiler, (struct piccolo_ExprNode*)hashmap->first);
                break;
            }
            case PICCOLO_EXPR_RANGE: {
                struct piccolo_RangeNode* range = (struct piccolo_RangeNode*)curr;
                findGlobals(engine, compiler, range->left);
                findGlobals(engine, compiler, range->right);
                break;
            }
            case PICCOLO_EXPR_SUBSCRIPT: {
                struct piccolo_SubscriptNode* subscript = (struct piccolo_SubscriptNode*)curr;
                findGlobals(engine, compiler, subscript->value);
                break;
            }
            case PICCOLO_EXPR_INDEX: {
                struct piccolo_IndexNode* index = (struct piccolo_IndexNode*)curr;
                findGlobals(engine, compiler, index->target);
                break;
            }
            case PICCOLO_EXPR_UNARY: {
                struct piccolo_UnaryNode* unary = (struct piccolo_UnaryNode*)curr;
                findGlobals(engine, compiler, unary->value);
                break;
            }
            case PICCOLO_EXPR_VAR_SET: {
                struct piccolo_VarSetNode* varSet = (struct piccolo_VarSetNode*)curr;
                findGlobals(engine, compiler, varSet->value);
                break;
            }
            case PICCOLO_EXPR_VAR_DECL: {
                struct piccolo_VarDeclNode* varDecl = (struct piccolo_VarDeclNode*)curr;
                if(getGlobalSlot(compiler, varDecl->name) != -1) {
                    compilationError(engine, compiler, varDecl->name.charIdx, "Variable '%.*s' already defined.", varDecl->name.length, varDecl->name.start);
                } else {
                    struct piccolo_Variable var = createVar(varDecl->name, compiler->globals->count);
                    var.Mutable = varDecl->mutable;
                    piccolo_writeVariableArray(engine, compiler->globals, var);
                }
                findGlobals(engine, compiler, varDecl->value);
                break;
            }
            case PICCOLO_EXPR_SUBSCRIPT_SET: {
                struct piccolo_SubscriptSetNode* subscriptSet = (struct piccolo_SubscriptSetNode*)curr;
                findGlobals(engine, compiler, subscriptSet->value);
                findGlobals(engine, compiler, subscriptSet->target);
                break;
            }
            case PICCOLO_EXPR_INDEX_SET: {
                struct piccolo_IndexSetNode* indexSet = (struct piccolo_IndexSetNode*)curr;
                findGlobals(engine, compiler, indexSet->target);
                findGlobals(engine, compiler, indexSet->value);
                break;
            }
            case PICCOLO_EXPR_IF: {
                struct piccolo_IfNode* ifNode = (struct piccolo_IfNode*)curr;
                findGlobals(engine, compiler, ifNode->condition);
                break;
            }
            case PICCOLO_EXPR_WHILE: {
                struct piccolo_WhileNode* whileNode = (struct piccolo_WhileNode*)curr;
                findGlobals(engine, compiler, whileNode->condition);
                break;
            }
            case PICCOLO_EXPR_FOR: {
                struct piccolo_ForNode* forNode = (struct piccolo_ForNode*)curr;
                findGlobals(engine, compiler, forNode->container);
                break;
            }
            case PICCOLO_EXPR_CALL: {
                struct piccolo_CallNode* callNode = (struct piccolo_CallNode*)curr;
                findGlobals(engine, compiler, callNode->function);
                break;
            }
            // TODO: These should be handled even though they may be error conditions since if someone is using piccolo from native unmanaged code they can create situations like this
            case PICCOLO_EXPR_LITERAL: break;
            case PICCOLO_EXPR_VAR: break;
            case PICCOLO_EXPR_BLOCK: break;
            case PICCOLO_EXPR_FN_LITERAL: break;
            case PICCOLO_EXPR_IMPORT: break;
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
    struct variableData varData = getVariable(engine, compiler, var->name);
    if(varData.slot == -1) {
        compilationError(engine, compiler, var->name.charIdx, "Variable '%.*s' is not defined.", var->name.length, var->name.start);
        return;
    }
    if(!var->expr.reqEval)
        return;
    piccolo_writeParameteredBytecode(engine, bytecode, varData.getOp, varData.slot, var->name.charIdx);
}

static void compileRange(struct piccolo_RangeNode* range, COMPILE_PARAMS) {
    compileExpr(range->left, COMPILE_ARGS);
    compileExpr(range->right, COMPILE_ARGS);
    if(range->expr.reqEval)
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_CREATE_RANGE, range->charIdx);
}

static void compileArrayLiteral(struct piccolo_ArrayLiteralNode* arrayLiteral, COMPILE_PARAMS) {
    struct piccolo_ExprNode* curr = arrayLiteral->first;
    int count = 0;
    while(curr != NULL) {
        count++;
        compileExpr(curr, COMPILE_ARGS);
        curr = curr->nextExpr;
    }
    if(arrayLiteral->expr.reqEval)
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CREATE_ARRAY, count, 0);
}

static void compileHashmapLiteral(struct piccolo_HashmapLiteralNode* hashmapLiteral, COMPILE_PARAMS) {
    if(hashmapLiteral->expr.reqEval)
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_HASHMAP, 0);
    struct piccolo_HashmapEntryNode* curr = hashmapLiteral->first;
    while(curr != NULL) {
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_PEEK_STACK, 1, 0);
        compileExpr(curr->key, COMPILE_ARGS);
        compileExpr(curr->value, COMPILE_ARGS);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SET_IDX, 0);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, 0);
        curr = (struct piccolo_HashmapEntryNode*)curr->expr.nextExpr;
    }
}

static void compileSubscript(struct piccolo_SubscriptNode* subscript, COMPILE_PARAMS) {
    compileExpr(subscript->value, COMPILE_ARGS);
    if(subscript->expr.reqEval) {
        struct piccolo_ObjString* subscriptStr = piccolo_copyString(engine, subscript->subscript.start, subscript->subscript.length);
        piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(subscriptStr), subscript->subscript.charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GET_IDX, subscript->subscript.charIdx);
    }
}

static void compileIndex(struct piccolo_IndexNode* index, COMPILE_PARAMS) {
    compileExpr(index->target, COMPILE_ARGS);
    compileExpr(index->index, COMPILE_ARGS);
    piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GET_IDX, index->charIdx);
}

static void compileUnary(struct piccolo_UnaryNode* unary, COMPILE_PARAMS) {
    compileExpr(unary->value, COMPILE_ARGS);
    if(!unary->value->reqEval)
        return;
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
       binary->op.type == PICCOLO_TOKEN_LESS_EQ ||
       binary->op.type == PICCOLO_TOKEN_IN) {
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
            case PICCOLO_TOKEN_IN: {
                op = PICCOLO_OP_IN;
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
    if(binary->op.type == PICCOLO_TOKEN_AND || binary->op.type == PICCOLO_TOKEN_OR) {
        compileExpr(binary->a, COMPILE_ARGS);
        if(binary->op.type == PICCOLO_TOKEN_OR) {
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_NOT, binary->op.charIdx);
        }
        int shortCircuitJumpAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP_FALSE, 0, binary->op.charIdx);
        compileExpr(binary->b, COMPILE_ARGS);
        int skipConstAddr = bytecode->code.count;
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP, 0, binary->op.charIdx);
        int constAddr = bytecode->code.count;
        piccolo_writeConst(engine, bytecode, PICCOLO_BOOL_VAL(binary->op.type == PICCOLO_TOKEN_OR), binary->op.charIdx);
        int endAddr = bytecode->code.count;

        piccolo_patchParam(bytecode, shortCircuitJumpAddr, constAddr - shortCircuitJumpAddr);
        piccolo_patchParam(bytecode, skipConstAddr, endAddr - skipConstAddr);
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
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_POP_LOCALS, compiler->locals.count - localCount, 0);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_CLOSE_UPVALS, 0);
        compiler->locals.count = localCount;
    }
}

static void compileFnLiteral(struct piccolo_FnLiteralNode* fnLiteral, COMPILE_PARAMS) {
    struct piccolo_Compiler fnCompiler;
    initCompiler(&fnCompiler, compiler->package, compiler->globals);
    fnCompiler.enclosing = compiler;
    for(int i = 0; i < fnLiteral->params.count; i++) {
        struct variableData varData = getVariable(engine, compiler, fnLiteral->params.values[i]);
        if(varData.slot == -1) {
            struct piccolo_Variable var = createVar(fnLiteral->params.values[i], fnCompiler.locals.count);
            piccolo_writeVariableArray(engine, &fnCompiler.locals, var);
        } else {
            compilationError(engine, compiler, fnLiteral->params.values[i].charIdx, "Variable '%.*s' already defined.", fnLiteral->params.values[i].length, fnLiteral->params.values[i].start);
        }
    }
    struct piccolo_ObjFunction* function = piccolo_newFunction(engine);
    function->arity = fnLiteral->params.count;
    compileExpr(fnLiteral->value, engine, &function->bytecode, &fnCompiler, false);
    piccolo_writeParameteredBytecode(engine, &function->bytecode, PICCOLO_OP_POP_LOCALS, fnLiteral->params.count, 0);
    piccolo_writeBytecode(engine, &function->bytecode, PICCOLO_OP_CLOSE_UPVALS, 0);
    piccolo_writeBytecode(engine, &function->bytecode, PICCOLO_OP_RETURN, 0);

    if(fnLiteral->expr.reqEval) {
        piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(function), 0);
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CLOSURE, fnCompiler.upvals.count, 0);
        for(int i = 0; i < fnCompiler.upvals.count; i++) {
            int slot = fnCompiler.upvals.values[i].slot;
            piccolo_writeBytecode(engine, bytecode, (slot & 0xFF00) >> 8, 0);
            piccolo_writeBytecode(engine, bytecode, (slot & 0x00FF) >> 0, 0);
            piccolo_writeBytecode(engine, bytecode, fnCompiler.upvals.values[i].local, 0);
        }
    }

    compiler->hadError |= fnCompiler.hadError;
}

static void compileVarDecl(struct piccolo_VarDeclNode* varDecl, COMPILE_PARAMS) {
    compileExpr(varDecl->value, COMPILE_ARGS);
    if(local) { // Act locally
        struct variableData varData = getVariable(engine, compiler, varDecl->name);
        if(varData.slot != -1) {
            compilationError(engine, compiler, varDecl->name.charIdx, "Variable '%.*s' already defined.", varDecl->name.length, varDecl->name.start);
        } else {
            int slot = compiler->locals.count;
            struct piccolo_Variable var = createVar(varDecl->name, slot);
            var.Mutable = varDecl->mutable;
            piccolo_writeVariableArray(engine, &compiler->locals, var);
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_PUSH_LOCAL, 0);
        }
    } else { // Think globally
        int slot = getGlobalSlot(compiler, varDecl->name);
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_SET_GLOBAL, slot, varDecl->name.charIdx);
    }
    if(!varDecl->expr.reqEval)
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, varDecl->name.charIdx);
}

static void compileVarSet(struct piccolo_VarSetNode* varSet, COMPILE_PARAMS) {
    struct variableData varData = getVariable(engine, compiler, varSet->name);
    compileExpr(varSet->value, COMPILE_ARGS);
    if(varData.slot == -1) {
        compilationError(engine, compiler, varSet->name.charIdx, "Variable '%.*s' is not defined.", varSet->name.length, varSet->name.start);
    } else {
        if(!varData.mutable) {
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

static void compileIndexSet(struct piccolo_IndexSetNode* indexSet, COMPILE_PARAMS) {
    compileExpr(indexSet->target, COMPILE_ARGS);
    compileExpr(indexSet->index, COMPILE_ARGS);
    compileExpr(indexSet->value, COMPILE_ARGS);
    piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SET_IDX, indexSet->charIdx);
    if(!indexSet->expr.reqEval)
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, indexSet->charIdx);
}

/*
    - condition -
    jump if false to skipTrue
    - trueExpr -
    jump to end
    skipTrue:
    - falseExpr(nil if not specified) -
    end:
*/
static void compileIf(struct piccolo_IfNode* ifNode, COMPILE_PARAMS) {
    compileExpr(ifNode->condition, COMPILE_ARGS);

    int skipTrueAddr = bytecode->code.count;
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP_FALSE, 0, ifNode->conditionCharIdx);
    compileExpr(ifNode->trueVal, COMPILE_ARGS);

    int skipFalseAddr = bytecode->code.count;
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP, 0, ifNode->conditionCharIdx);

    int falseStartAddr = bytecode->code.count;
    piccolo_patchParam(bytecode, skipTrueAddr, falseStartAddr - skipTrueAddr);

    if(ifNode->falseVal != NULL) {
        compileExpr(ifNode->falseVal, COMPILE_ARGS);
    } else {
        if(ifNode->expr.reqEval)
            piccolo_writeConst(engine, bytecode, PICCOLO_NIL_VAL(), 0);
    }

    int endAddr = bytecode->code.count;
    piccolo_patchParam(bytecode, skipFalseAddr, endAddr - skipFalseAddr);
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

static void compileFor(struct piccolo_ForNode* forNode, COMPILE_PARAMS) {
    /*

        Not req val:

            container idx
            container idx container
            container idx container idx
            container idx element
            container idx
            - for loop body -
            container idx
            container idx 1
            container (idx + 1)
        

        Req Val:

            container idx result
            container idx result container
            container idx result container idx
            container idx result element
            container idx result
            - for loop body -
            container idx result bodyVal
            container idx (result + bodyVal)
            container (result + bodyVal) idx
            container (result + bodyVal) idx 1
            container (result + bodyVal) (idx + 1)
            container (idx + 1) (result + bodyVal)

    */

    struct variableData varData = getVariable(engine, compiler, forNode->name);
    int slot = -1;
    if(varData.slot != -1) {
        compilationError(engine, compiler, forNode->name.charIdx, "Variable %.*s is already defined", forNode->name.length, forNode->name.start);
    } else {
        slot = compiler->locals.count;
        struct piccolo_Variable var = createVar(forNode->name, slot);
        var.Mutable = false;
        piccolo_writeVariableArray(engine, &compiler->locals, var);
        piccolo_writeConst(engine, bytecode, PICCOLO_NIL_VAL(), 0);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_PUSH_LOCAL, 0);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, 0);
    }

    compileExpr(forNode->container, COMPILE_ARGS); // container
    piccolo_writeConst(engine, bytecode, PICCOLO_NUM_VAL(0), forNode->charIdx); // idx
    if(forNode->expr.reqEval)
        piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CREATE_ARRAY, 0, forNode->charIdx); // result

    int peekDist = forNode->expr.reqEval ? 3 : 2;
    int loopStartAddr = bytecode->code.count;
    
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_PEEK_STACK, peekDist, forNode->charIdx);
    piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GET_LEN, forNode->containerCharIdx);
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_PEEK_STACK, peekDist, forNode->charIdx);
    piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GREATER, forNode->charIdx);
    int breakLoopAddr = bytecode->code.count;
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_JUMP_FALSE, 0, forNode->charIdx);

    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_PEEK_STACK, peekDist, forNode->charIdx); // container
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_PEEK_STACK, peekDist, forNode->charIdx); // idx
    piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_GET_IDX, forNode->charIdx); // element
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_SET_LOCAL, slot, forNode->charIdx);
    piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, forNode->charIdx);

    compileExpr(forNode->value, COMPILE_ARGS);

    if(forNode->expr.reqEval) {
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_APPEND, forNode->charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SWAP_STACK, forNode->charIdx);
    }
    piccolo_writeConst(engine, bytecode, PICCOLO_NUM_VAL(1), 0);
    piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_ADD, 0);
    if(forNode->expr.reqEval) {
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SWAP_STACK, forNode->charIdx);
    }

    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_REV_JUMP, bytecode->code.count - loopStartAddr, 0);

    int loopEndAddr = bytecode->code.count;
    if(forNode->expr.reqEval) {
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SWAP_STACK, forNode->charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, forNode->charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_SWAP_STACK, forNode->charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, forNode->charIdx);
    } else {
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, forNode->charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, forNode->charIdx);
    }
    
    piccolo_patchParam(bytecode, breakLoopAddr, loopEndAddr - breakLoopAddr);

    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_POP_LOCALS, 1, 0);

    if(varData.slot == -1) {
        compiler->locals.count--;
    }

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
    size_t packageLen = import->packageName.length - 2;
    char* packageName = strndup(import->packageName.start + 1, packageLen);
    struct piccolo_Package *package = resolvePackage(engine, compiler, compiler->package->packageName, packageName, packageLen);
    if(package == NULL) {
        compilationError(engine, compiler, import->packageName.charIdx, "Package %.*s does not exist.", import->packageName.length, import->packageName.start);
    } else {
        piccolo_writeConst(engine, bytecode, PICCOLO_OBJ_VAL(package), import->packageName.charIdx);
        piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_EXECUTE_PACKAGE, import->packageName.charIdx);
        if(!import->expr.reqEval)
            piccolo_writeBytecode(engine, bytecode, PICCOLO_OP_POP_STACK, import->packageName.charIdx);
    }
    free(packageName);
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
        case PICCOLO_EXPR_RANGE: {
            compileRange((struct piccolo_RangeNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_ARRAY_LITERAL: {
            compileArrayLiteral((struct piccolo_ArrayLiteralNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_HASHMAP_LITERAL: {
            compileHashmapLiteral((struct piccolo_HashmapLiteralNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_HASHMAP_ENTRY: break; // TODO: Handle this too
        case PICCOLO_EXPR_SUBSCRIPT: {
            compileSubscript((struct piccolo_SubscriptNode*)expr, COMPILE_ARGS);
            break;
        }
        case PICCOLO_EXPR_INDEX: {
            compileIndex((struct piccolo_IndexNode*)expr, COMPILE_ARGS);
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
        case PICCOLO_EXPR_FN_LITERAL: {
            compileFnLiteral((struct piccolo_FnLiteralNode*)expr, COMPILE_ARGS);
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
        case PICCOLO_EXPR_INDEX_SET: {
            compileIndexSet((struct piccolo_IndexSetNode*)expr, COMPILE_ARGS);
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
        case PICCOLO_EXPR_FOR: {
            compileFor((struct piccolo_ForNode*)expr, COMPILE_ARGS);
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
    piccolo_initParser(engine, &parser, &scanner, package);
    struct piccolo_ExprNode* ast = piccolo_parse(engine, &parser);

    if(parser.hadError) {
        piccolo_freeParser(engine, &parser);
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
    for(int i = 0; i < globals.count; i++) {
        struct piccolo_ObjString* name = piccolo_copyString(engine, globals.values[i].nameStart, globals.values[i].nameLen);
        piccolo_setGlobalTable(engine, &package->globalIdxs, name, globals.values[i].slot | (PICCOLO_GLOBAL_SLOT_MUTABLE_BIT * globals.values[i].Mutable));
    }

    currExpr = ast;
    while(currExpr != NULL) {
        compileExpr(currExpr, engine, &package->bytecode, &compiler, false);
        currExpr = currExpr->nextExpr;
    }

    piccolo_printExpr(ast, 0);
    piccolo_freeParser(engine, &parser);

    piccolo_writeBytecode(engine, &package->bytecode, PICCOLO_OP_RETURN, 0);
    piccolo_disassembleBytecode(&package->bytecode);

    package->compiled = true;

    return !compiler.hadError;
}

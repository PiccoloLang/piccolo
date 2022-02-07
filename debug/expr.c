
#include "expr.h"
#include "../typecheck.h"

#include <stdio.h>

void piccolo_printExpr(struct piccolo_ExprNode* expr, int offset) {
    if(expr == NULL)
        return;

    char typeBuf[256];
    piccolo_getTypename(expr->resultType, typeBuf);

    printf(expr->reqEval ? "[X] " : "[ ] ");
    for(int i = 0; i < offset; i++)
        printf("    ");
    switch(expr->type) {
        case PICCOLO_EXPR_LITERAL: {
            struct piccolo_LiteralNode* literal = (struct piccolo_LiteralNode*)expr;
            printf("LITERAL %.*s -> %s\n", (int)literal->token.length, literal->token.start, typeBuf);
            break;
        }
        case PICCOLO_EXPR_VAR: {
            struct piccolo_VarNode* var = (struct piccolo_VarNode*)expr;
            printf("VAR %.*s -> %s\n", (int)var->name.length, var->name.start, typeBuf);
            break;
        }
        case PICCOLO_EXPR_RANGE: {
            struct piccolo_RangeNode* range = (struct piccolo_RangeNode*)expr;
            printf("RANGE -> %s\n", typeBuf);
            piccolo_printExpr(range->left, offset + 1);
            piccolo_printExpr(range->right, offset + 1);
            break;
        }
        case PICCOLO_EXPR_ARRAY_LITERAL: {
            struct piccolo_ArrayLiteralNode* arrayLiteral = (struct piccolo_ArrayLiteralNode*)expr;
            printf("ARRAY LITERAL -> %s\n", typeBuf);
            piccolo_printExpr(arrayLiteral->first, offset + 1);
            break;
        }
        case PICCOLO_EXPR_HASHMAP_ENTRY: {
            struct piccolo_HashmapEntryNode* entry = (struct piccolo_HashmapEntryNode*)expr;
            printf("HASHMAP ENTRY\n");
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("KEY ->\n");
            piccolo_printExpr(entry->key, offset + 2);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("VALUE ->\n");
            piccolo_printExpr(entry->value, offset + 2);
            break;
        }
        case PICCOLO_EXPR_HASHMAP_LITERAL: {
            struct piccolo_HashmapLiteralNode* hashmap = (struct piccolo_HashmapLiteralNode*)expr;
            printf("HASHMAP -> %s\n", typeBuf);
            piccolo_printExpr((struct piccolo_ExprNode*)hashmap->first, offset + 1);
            break;
        }
        case PICCOLO_EXPR_SUBSCRIPT: {
            struct piccolo_SubscriptNode* subscript = (struct piccolo_SubscriptNode*)expr;
            printf("SUBSCRIPT %.*s -> %s\n", (int)subscript->subscript.length, subscript->subscript.start, typeBuf);
            piccolo_printExpr(subscript->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_INDEX: {
            struct piccolo_IndexNode* index = (struct piccolo_IndexNode*)expr;
            printf("INDEX -> %s\n", typeBuf);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("VALUE\n");
            piccolo_printExpr(index->target, offset + 2);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("INDEX\n");
            piccolo_printExpr(index->index, offset + 2);
            break;
        }
        case PICCOLO_EXPR_UNARY: {
            struct piccolo_UnaryNode* unary = (struct piccolo_UnaryNode*)expr;
            printf("UNARY %.*s -> %s\n", (int)unary->op.length, unary->op.start, typeBuf);
            piccolo_printExpr(unary->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_BINARY: {
            struct piccolo_BinaryNode* binary = (struct piccolo_BinaryNode*)expr;
            printf("BINARY %.*s -> %s\n", (int)binary->op.length, binary->op.start, typeBuf);
            piccolo_printExpr(binary->a, offset + 1);
            piccolo_printExpr(binary->b, offset + 1);
            break;
        }
        case PICCOLO_EXPR_BLOCK: {
            struct piccolo_BlockNode* block = (struct piccolo_BlockNode*)expr;
            printf("BLOCK -> %s\n", typeBuf);
            piccolo_printExpr(block->first, offset + 1);
            break;
        }
        case PICCOLO_EXPR_FN_LITERAL: {
            struct piccolo_FnLiteralNode* fnLiteral = (struct piccolo_FnLiteralNode*)expr;
            printf("FN_LITERAL ");
            for(int i = 0; i < fnLiteral->params.count; i++)
                printf("%.*s ", (int)fnLiteral->params.values[i].length, fnLiteral->params.values[i].start);
            printf(" -> %s\n", typeBuf);
            piccolo_printExpr(fnLiteral->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_VAR_DECL: {
            struct piccolo_VarDeclNode* varDecl = (struct piccolo_VarDeclNode*)expr;
            printf("VAR DECL %.*s -> %s\n", (int)varDecl->name.length, varDecl->name.start, typeBuf);
            piccolo_printExpr(varDecl->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_VAR_SET: {
            struct piccolo_VarSetNode* varSet = (struct piccolo_VarSetNode*)expr;
            printf("VAR SET %.*s -> %s\n", (int)varSet->name.length, varSet->name.start, typeBuf);
            piccolo_printExpr(varSet->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_SUBSCRIPT_SET: {
            struct piccolo_SubscriptSetNode* subscriptSet = (struct piccolo_SubscriptSetNode*)expr;
            printf("SUBSCRIPT SET %.*s -> %s\n", (int)subscriptSet->subscript.length, subscriptSet->subscript.start, typeBuf);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("TARGET\n");
            piccolo_printExpr(subscriptSet->target, offset + 2);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("VALUE\n");
            piccolo_printExpr(subscriptSet->value, offset + 2);
            break;
        }
        case PICCOLO_EXPR_INDEX_SET: {
            struct piccolo_IndexSetNode* indexSet = (struct piccolo_IndexSetNode*)expr;
            printf("INDEX SET -> %s\n", typeBuf);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("TARGET\n");
            piccolo_printExpr(indexSet->target, offset + 2);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("INDEX\n");
            piccolo_printExpr(indexSet->index, offset + 2);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("VALUE\n");
            piccolo_printExpr(indexSet->value, offset + 2);
            break;
        }
        case PICCOLO_EXPR_IF: {
            struct piccolo_IfNode* ifNode = (struct piccolo_IfNode*)expr;
            printf("IF -> %s\n", typeBuf);
            piccolo_printExpr(ifNode->condition, offset + 1);
            for(int i = 0; i <= offset + 1; i++)
                printf("    ");
            printf("TRUE EXPR\n");
            piccolo_printExpr(ifNode->trueVal, offset + 2);
            if(ifNode->falseVal != NULL) {
                for(int i = 0; i <= offset + 1; i++)
                    printf("    ");
                printf("FALSE EXPR\n");
                piccolo_printExpr(ifNode->falseVal, offset + 2);
            }
            break;
        }
        case PICCOLO_EXPR_WHILE: {
            struct piccolo_WhileNode* whileNode = (struct piccolo_WhileNode*)expr;
            printf("WHILE -> %s\n", typeBuf);
            for(int i = 0; i <= offset + 1; i++)
                printf("    ");
            printf("CONDITION\n");
            piccolo_printExpr(whileNode->condition, offset + 2);
            for(int i = 0; i <= offset + 1; i++)
                printf("    ");
            printf("VALUE\n");
            piccolo_printExpr(whileNode->value, offset + 2);
            break;
        }
        case PICCOLO_EXPR_FOR: {
            struct piccolo_ForNode* forNode = (struct piccolo_ForNode*)expr;
            printf("FOR %.*s -> %s\n", (int)forNode->name.length, forNode->name.start, typeBuf);
            for(int i = 0; i <= offset + 1; i++)
                printf("    ");
            printf("IN\n");
            piccolo_printExpr(forNode->container, offset + 2);
            for(int i = 0; i <= offset + 1; i++)
                printf("    ");
            printf("VALUE\n");
            piccolo_printExpr(forNode->value, offset + 2);
            break;
        }
        case PICCOLO_EXPR_CALL: {
            struct piccolo_CallNode* call = (struct piccolo_CallNode*)expr;
            printf("CALL -> %s\n", typeBuf);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("FUNC\n");
            piccolo_printExpr(call->function, offset + 2);
            for(int i = 0; i < offset + 2; i++)
                printf("    ");
            printf("ARGS\n");
            piccolo_printExpr(call->firstArg, offset + 2);
            break;
        }
        case PICCOLO_EXPR_IMPORT: {
            struct piccolo_ImportNode* import = (struct piccolo_ImportNode*)expr;
            printf("IMPORT %.*s -> %s\n", (int)import->packageName.length, import->packageName.start, typeBuf);
            break;
        }
        default: {
            printf("UNKNOWN AST TYPE\n");
        }
    }

    if(expr->nextExpr != NULL) {
        piccolo_printExpr(expr->nextExpr, offset);
    }
}

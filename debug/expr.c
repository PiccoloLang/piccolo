
#include "expr.h"

#include <stdio.h>

void piccolo_printExpr(struct piccolo_ExprNode* expr, int offset) {
    if(expr == NULL)
        return;

    printf(expr->reqEval ? "[X] " : "[ ] ");
    for(int i = 0; i < offset; i++)
        printf("    ");
    switch(expr->type) {
        case PICCOLO_EXPR_LITERAL: {
            struct piccolo_LiteralNode* literal = (struct piccolo_LiteralNode*)expr;
            printf("LITERAL %.*s\n", (int)literal->token.length, literal->token.start);
            break;
        }
        case PICCOLO_EXPR_VAR: {
            struct piccolo_VarNode* var = (struct piccolo_VarNode*)expr;
            printf("VAR %.*s\n", (int)var->name.length, var->name.start);
            break;
        }
        case PICCOLO_EXPR_RANGE: {
            struct piccolo_RangeNode* range = (struct piccolo_RangeNode*)expr;
            printf("RANGE\n");
            piccolo_printExpr(range->left, offset + 1);
            piccolo_printExpr(range->right, offset + 1);
            break;
        }
        case PICCOLO_EXPR_ARRAY_LITERAL: {
            struct piccolo_ArrayLiteralNode* arrayLiteral = (struct piccolo_ArrayLiteralNode*)expr;
            printf("ARRAY LITERAL\n");
            piccolo_printExpr(arrayLiteral->first, offset + 1);
            break;
        }
        case PICCOLO_EXPR_SUBSCRIPT: {
            struct piccolo_SubscriptNode* subscript = (struct piccolo_SubscriptNode*)expr;
            printf("SUBSCRIPT %.*s\n", (int)subscript->subscript.length, subscript->subscript.start);
            piccolo_printExpr(subscript->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_INDEX: {
            struct piccolo_IndexNode* index = (struct piccolo_IndexNode*)expr;
            printf("INDEX\n");
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
            printf("UNARY %.*s\n", (int)unary->op.length, unary->op.start);
            piccolo_printExpr(unary->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_BINARY: {
            struct piccolo_BinaryNode* binary = (struct piccolo_BinaryNode*)expr;
            printf("BINARY %.*s\n", (int)binary->op.length, binary->op.start);
            piccolo_printExpr(binary->a, offset + 1);
            piccolo_printExpr(binary->b, offset + 1);
            break;
        }
        case PICCOLO_EXPR_BLOCK: {
            struct piccolo_BlockNode* block = (struct piccolo_BlockNode*)expr;
            printf("BLOCK\n");
            piccolo_printExpr(block->first, offset + 1);
            break;
        }
        case PICCOLO_EXPR_FN_LITERAL: {
            struct piccolo_FnLiteralNode* fnLiteral = (struct piccolo_FnLiteralNode*)expr;
            printf("FN_LITERAL ");
            for(int i = 0; i < fnLiteral->params.count; i++)
                printf("%.*s ", (int)fnLiteral->params.values[i].length, fnLiteral->params.values[i].start);
            printf("\n");
            piccolo_printExpr(fnLiteral->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_VAR_DECL: {
            struct piccolo_VarDeclNode* varDecl = (struct piccolo_VarDeclNode*)expr;
            printf("VAR DECL %.*s\n", (int)varDecl->name.length, varDecl->name.start);
            piccolo_printExpr(varDecl->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_VAR_SET: {
            struct piccolo_VarSetNode* varSet = (struct piccolo_VarSetNode*)expr;
            printf("VAR SET %.*s\n", (int)varSet->name.length, varSet->name.start);
            piccolo_printExpr(varSet->value, offset + 1);
            break;
        }
        case PICCOLO_EXPR_SUBSCRIPT_SET: {
            struct piccolo_SubscriptSetNode* subscriptSet = (struct piccolo_SubscriptSetNode*)expr;
            printf("SUBSCRIPT SET %.*s\n", (int)subscriptSet->subscript.length, subscriptSet->subscript.start);
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
            printf("INDEX SET\n");
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
            printf("IF\n");
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
            printf("WHILE\n");
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
            printf("FOR %.*s\n", (int)forNode->name.length, forNode->name.start);
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
            printf("CALL\n");
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
            printf("IMPORT %.*s\n", (int)import->packageName.length, import->packageName.start);
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
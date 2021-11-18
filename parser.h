
#ifndef PICCOLO_PARSER_H
#define PICCOLO_PARSER_H

#include "scanner.h"
#include "engine.h"

enum piccolo_ExprNodeType {
    PICCOLO_EXPR_LITERAL,
    PICCOLO_EXPR_VAR,
    PICCOLO_EXPR_SUBSCRIPT,
    PICCOLO_EXPR_UNARY,
    PICCOLO_EXPR_BINARY,
    PICCOLO_EXPR_BLOCK,

    PICCOLO_EXPR_VAR_DECL,
    PICCOLO_EXPR_VAR_SET,
    PICCOLO_EXPR_SUBSCRIPT_SET,

    PICCOLO_EXPR_IF,

    PICCOLO_EXPR_CALL,

    PICCOLO_EXPR_IMPORT,
};

struct piccolo_ExprNode {
    struct piccolo_ExprNode* nodes;
    struct piccolo_ExprNode* nextExpr;
    enum piccolo_ExprNodeType type;
    bool reqEval;
};

struct piccolo_LiteralNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token token;
};

struct piccolo_VarNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token name;
};

struct piccolo_SubscriptNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token subscript;
    struct piccolo_ExprNode* value;
};

struct piccolo_UnaryNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token op;
    struct piccolo_ExprNode* value;
};

struct piccolo_BinaryNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token op;
    struct piccolo_ExprNode* a;
    struct piccolo_ExprNode* b;
};

struct piccolo_BlockNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* first;
};

struct piccolo_VarDeclNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token name;
    struct piccolo_ExprNode* value;
    bool mutable;
};

struct piccolo_VarSetNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token name;
    struct piccolo_ExprNode* value;
};

struct piccolo_SubscriptSetNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* target;
    struct piccolo_Token subscript;
    struct piccolo_ExprNode* value;
};

struct piccolo_IfNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* condition;
    struct piccolo_ExprNode* trueVal;
    struct piccolo_ExprNode* falseVal;
    int conditionCharIdx;
};

struct piccolo_CallNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* function;
    struct piccolo_ExprNode* firstArg;
    int charIdx;
};

struct piccolo_ImportNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token packageName;
};

struct piccolo_Parser {
    struct piccolo_Scanner* scanner;
    struct piccolo_ExprNode* nodes;
    struct piccolo_Token currToken;
    bool cycled, hadError;
};

void piccolo_initParser(struct piccolo_Engine* engine, struct piccolo_Parser* parser, struct piccolo_Scanner* scanner);
void piccolo_freeParser(struct piccolo_Parser* parser);

struct piccolo_ExprNode* piccolo_parse(struct piccolo_Engine* engine, struct piccolo_Parser* parser);

#endif

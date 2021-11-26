
#ifndef PICCOLO_PARSER_H
#define PICCOLO_PARSER_H

#include "scanner.h"
#include "engine.h"
#include "util/dynarray.h"

PICCOLO_DYNARRAY_HEADER(struct piccolo_Token, Token)

enum piccolo_ExprNodeType {
    PICCOLO_EXPR_LITERAL,
    PICCOLO_EXPR_ARRAY_LITERAL,
    PICCOLO_EXPR_HASHMAP_ENTRY,
    PICCOLO_EXPR_HASHMAP_LITERAL,
    PICCOLO_EXPR_VAR,
    PICCOLO_EXPR_RANGE,
    PICCOLO_EXPR_SUBSCRIPT,
    PICCOLO_EXPR_INDEX,
    PICCOLO_EXPR_UNARY,
    PICCOLO_EXPR_BINARY,
    PICCOLO_EXPR_BLOCK,
    PICCOLO_EXPR_FN_LITERAL,

    PICCOLO_EXPR_VAR_DECL,
    PICCOLO_EXPR_VAR_SET,
    PICCOLO_EXPR_SUBSCRIPT_SET,
    PICCOLO_EXPR_INDEX_SET,

    PICCOLO_EXPR_IF,
    PICCOLO_EXPR_WHILE,
    PICCOLO_EXPR_FOR,

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

struct piccolo_ArrayLiteralNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* first;
};

struct piccolo_HashmapEntryNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* key;
    struct piccolo_ExprNode* value;
};

struct piccolo_HashmapLiteralNode {
    struct piccolo_ExprNode expr;
    struct piccolo_HashmapEntryNode* first;
};

struct piccolo_VarNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token name;
};

struct piccolo_RangeNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* left;
    struct piccolo_ExprNode* right;
    int charIdx;
};

struct piccolo_SubscriptNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token subscript;
    struct piccolo_ExprNode* value;
};

struct piccolo_IndexNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* target;
    struct piccolo_ExprNode* index;
    int charIdx;
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

struct piccolo_FnLiteralNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* value;
    struct piccolo_TokenArray params;
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

struct piccolo_IndexSetNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* target;
    struct piccolo_ExprNode* index;
    struct piccolo_ExprNode* value;
    int charIdx;
};

struct piccolo_IfNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* condition;
    struct piccolo_ExprNode* trueVal;
    struct piccolo_ExprNode* falseVal;
    int conditionCharIdx;
};

struct piccolo_WhileNode {
    struct piccolo_ExprNode expr;
    struct piccolo_ExprNode* condition;
    struct piccolo_ExprNode* value;
    int conditionCharIdx;
};

struct piccolo_ForNode {
    struct piccolo_ExprNode expr;
    struct piccolo_Token name;
    struct piccolo_ExprNode* container;
    struct piccolo_ExprNode* value;
    int charIdx;
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
    struct piccolo_Package* package;
};

void piccolo_initParser(struct piccolo_Engine* engine, struct piccolo_Parser* parser, struct piccolo_Scanner* scanner, struct piccolo_Package* package);
void piccolo_freeParser(struct piccolo_Engine* engine, struct piccolo_Parser* parser);

struct piccolo_ExprNode* piccolo_parse(struct piccolo_Engine* engine, struct piccolo_Parser* parser);

#endif

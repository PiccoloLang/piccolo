
#include "parser.h"
#include "engine.h"
#include "util/strutil.h"

#include <stdlib.h>
#include <stdarg.h>

PICCOLO_DYNARRAY_IMPL(struct piccolo_Token, Token)

static struct piccolo_ExprNode* createNode(struct piccolo_Parser* parser, size_t size, enum piccolo_ExprNodeType type) {
    struct piccolo_ExprNode* node = malloc(size);
    node->nodes = parser->nodes;
    node->nextExpr = NULL;
    node->type = type;
    node->reqEval = false;
    parser->nodes = node;
    return node;
}

#define ALLOCATE_NODE(parser, name, type) \
    ((struct piccolo_ ## name ## Node*)createNode(parser, sizeof(struct piccolo_ ## name ## Node), type))

static void parsingError(struct piccolo_Engine* engine, struct piccolo_Parser* parser, const char* format, ...) {
    va_list args;
    va_start(args, format);
    engine->printError(format, args);
    va_end(args);
    piccolo_enginePrintError(engine, " [%s]", parser->package->packageName);
    int charIdx = parser->currToken.charIdx;
    struct piccolo_strutil_LineInfo line = piccolo_strutil_getLine(parser->scanner->source, charIdx);
    piccolo_enginePrintError(engine, "\n[line %d] %.*s\n", line.line + 1, line.lineEnd - line.lineStart, line.lineStart);

    int lineNumberDigits = 0;
    int lineNumber = line.line + 1;
    while(lineNumber > 0) {
        lineNumberDigits++;
        lineNumber /= 10;
    }
    piccolo_enginePrintError(engine, "%*c ^", 7 + lineNumberDigits + parser->currToken.start - line.lineStart, ' ');
    piccolo_enginePrintError(engine, "\n");

    parser->hadError = true;
}

static void advanceParser(struct piccolo_Engine* engine, struct piccolo_Parser* parser) {
    parser->currToken = piccolo_nextToken(parser->scanner);
    parser->cycled = false;
    while(parser->currToken.type == PICCOLO_TOKEN_ERROR) {
        parsingError(engine, parser, "Malformed token.");
        parser->currToken = piccolo_nextToken(parser->scanner);
    }
}


/*
    The reqExpr parameter is needed for determining whether newlines are significant.
    For example, in a case like this:

    4 +
    2

    The newline is not treated as significant, because an expression is required
    after the plus.

    In a case like this, however:

    4
    -3

    The newline is treated as significant, because an expression is not needed
    after the 4.
*/
#define PARSER_PARAMS struct piccolo_Engine* engine, struct piccolo_Parser* parser, bool reqExpr
#define PARSER_ARGS engine, parser, false
#define PARSER_ARGS_REQ_VAL engine, parser, true

#define SKIP_NEWLINES() \
    if(reqExpr) { \
        while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE) \
            advanceParser(engine, parser);  \
    }

static struct piccolo_ExprNode* parseExpr(PARSER_PARAMS);

struct piccolo_ExprNode* parseExprList(struct piccolo_Engine* engine, struct piccolo_Parser* parser, bool allowRightBrace) {
    struct piccolo_ExprNode* first = NULL;
    struct piccolo_ExprNode* curr = NULL;

    while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
        advanceParser(engine, parser);

    while(parser->currToken.type != PICCOLO_TOKEN_EOF && (!allowRightBrace || parser->currToken.type != PICCOLO_TOKEN_RIGHT_BRACE)) {
        struct piccolo_ExprNode* node = parseExpr(engine, parser, false);
        while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE) {
            advanceParser(engine, parser);
        }

        if(node != NULL) {
            if (first == NULL)
                first = node;

            if (curr == NULL) {
                curr = node;
            } else {
                curr->nextExpr = node;
                curr = node;
            }
        }
    }
    return first;
}

static struct piccolo_ExprNode* parseBlock(PARSER_PARAMS) {
    advanceParser(engine, parser);
    while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
        advanceParser(engine, parser);
    
    if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_BRACE) {
        advanceParser(engine, parser);
        struct piccolo_HashmapLiteralNode* hashmap = ALLOCATE_NODE(parser, HashmapLiteral, PICCOLO_EXPR_HASHMAP_LITERAL);
        hashmap->first = NULL;
        return (struct piccolo_ExprNode*)hashmap;
    }

    struct piccolo_ExprNode* firstExpr = parseExpr(PARSER_ARGS);
    if(firstExpr == NULL) return NULL;
    while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
        advanceParser(engine, parser);
    
    if(parser->currToken.type == PICCOLO_TOKEN_COLON) {
        advanceParser(engine, parser);
        struct piccolo_ExprNode* firstValue = parseExpr(PARSER_ARGS_REQ_VAL);
        while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
            advanceParser(engine, parser);
        if(parser->currToken.type == PICCOLO_TOKEN_COMMA) {
            advanceParser(engine, parser);
            while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
                advanceParser(engine, parser);
            if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_BRACE) {
                parsingError(engine, parser, "Expected expression.");
            }
        } else if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_BRACE) {

        } else {
            parsingError(engine, parser, "Expected comma.");
        }
        struct piccolo_HashmapEntryNode* firstEntry = ALLOCATE_NODE(parser, HashmapEntry, PICCOLO_EXPR_HASHMAP_ENTRY);
        firstEntry->key = firstExpr;
        firstEntry->value = firstValue;
        struct piccolo_HashmapEntryNode* curr = firstEntry;

        while(parser->currToken.type != PICCOLO_TOKEN_RIGHT_BRACE) {
            if(parser->currToken.type == PICCOLO_TOKEN_EOF) {
                parsingError(engine, parser, "Expected }.");
                break;
            }

            struct piccolo_ExprNode* key = parseExpr(PARSER_ARGS_REQ_VAL);
            while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
                advanceParser(engine, parser);
            if(parser->currToken.type == PICCOLO_TOKEN_COLON) {
                advanceParser(engine, parser);
            } else {
                parsingError(engine, parser, "Expected :.");
            }
            if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_BRACE) {
                parsingError(engine, parser, "Expected expression.");
                break;
            }
            struct piccolo_ExprNode* value = parseExpr(PARSER_ARGS_REQ_VAL);
            while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
                advanceParser(engine, parser);

            struct piccolo_HashmapEntryNode* entry = ALLOCATE_NODE(parser, HashmapEntry, PICCOLO_EXPR_HASHMAP_ENTRY);
            entry->expr.nextExpr = NULL;
            entry->key = key;
            entry->value = value;
            curr->expr.nextExpr = (struct piccolo_ExprNode*)entry;
            curr = entry;

            while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
                advanceParser(engine, parser);
            if(parser->currToken.type == PICCOLO_TOKEN_COMMA) {
                advanceParser(engine, parser);
                while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
                    advanceParser(engine, parser);
                if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_BRACE) {
                    parsingError(engine, parser, "Expected expression.");
                }
            } else if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_BRACE) {

            } else {
                parsingError(engine, parser, "Expected comma.");
            }
        }
        advanceParser(engine, parser);

        struct piccolo_HashmapLiteralNode* hashmap = ALLOCATE_NODE(parser, HashmapLiteral, PICCOLO_EXPR_HASHMAP_LITERAL);
        hashmap->first = firstEntry;
        
        return (struct piccolo_ExprNode*)hashmap;

    } else {
        struct piccolo_ExprNode* exprs = parseExprList(engine, parser, true);
        if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_BRACE) {
            advanceParser(engine, parser);
        } else {
            parsingError(engine, parser, "Expected }.");
        }
        struct piccolo_BlockNode* block = ALLOCATE_NODE(parser, Block, PICCOLO_EXPR_BLOCK);
        firstExpr->nextExpr = exprs;
        block->first = firstExpr;
        return (struct piccolo_ExprNode*)block;
    }
    return NULL;
}

static struct piccolo_ExprNode* parseLiteral(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_NUM ||
       parser->currToken.type == PICCOLO_TOKEN_STRING ||
       parser->currToken.type == PICCOLO_TOKEN_TRUE ||
       parser->currToken.type == PICCOLO_TOKEN_FALSE ||
       parser->currToken.type == PICCOLO_TOKEN_NIL) {
        struct piccolo_LiteralNode* literal = ALLOCATE_NODE(parser, Literal, PICCOLO_EXPR_LITERAL);
        literal->token = parser->currToken;
        advanceParser(engine, parser);
        return (struct piccolo_ExprNode*)literal;
    }
    if(parser->currToken.type == PICCOLO_TOKEN_LEFT_PAREN) {
        advanceParser(engine, parser);
        struct piccolo_ExprNode* value = parseExpr(PARSER_ARGS_REQ_VAL);

        while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
            advanceParser(engine, parser);

        if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_PAREN) {
            advanceParser(engine, parser);
        } else {
            parsingError(engine, parser, "Expected ).");
        }
        return value;
    }
    if(parser->currToken.type == PICCOLO_TOKEN_LEFT_BRACE) {
        return parseBlock(PARSER_ARGS);
    }

    if(parser->cycled) {
        parsingError(engine, parser, "Expected expression.");
        advanceParser(engine, parser);
    } else {
        parser->cycled = true;
        return parseExpr(PARSER_ARGS);
    }
    return NULL;
}

static struct piccolo_ExprNode* parseVar(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_IDENTIFIER) {
        struct piccolo_Token varName = parser->currToken;
        advanceParser(engine, parser);
        if(parser->currToken.type == PICCOLO_TOKEN_EQ) {
            struct piccolo_VarSetNode* varSet = ALLOCATE_NODE(parser, VarSet, PICCOLO_EXPR_VAR_SET);
            varSet->name = varName;
            advanceParser(engine, parser);
            varSet->value = parseExpr(PARSER_ARGS_REQ_VAL);
            return (struct piccolo_ExprNode*)varSet;
        } else {
            struct piccolo_VarNode* var = ALLOCATE_NODE(parser, Var, PICCOLO_EXPR_VAR);
            var->name = varName;
            return (struct piccolo_ExprNode*)var;
        }
    }
    return parseLiteral(PARSER_ARGS);
}


static struct piccolo_ExprNode* parseArrayLiteral(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_LEFT_SQR_PAREN) {
        advanceParser(engine, parser);
        while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
            advanceParser(engine, parser);
        struct piccolo_ExprNode* first = NULL;
        struct piccolo_ExprNode* curr = NULL;
        while(parser->currToken.type != PICCOLO_TOKEN_RIGHT_SQR_PAREN) {
            if(parser->currToken.type == PICCOLO_TOKEN_EOF) {
                parsingError(engine, parser, "Expected ].");
                break;
            }
            struct piccolo_ExprNode* next = parseExpr(PARSER_ARGS_REQ_VAL);
            while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
                advanceParser(engine, parser);
            if(first == NULL)
                first = next;
            if(curr != NULL)
                curr->nextExpr = next;
            curr = next;

            if(parser->currToken.type == PICCOLO_TOKEN_COMMA) {
                advanceParser(engine, parser);
                if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_SQR_PAREN) {
                    parsingError(engine, parser, "Expected expression.");
                }
            } else if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_SQR_PAREN) {

            } else {
                parsingError(engine, parser, "Expected comma.");
                advanceParser(engine, parser);
            }
        }
        advanceParser(engine, parser);
        struct piccolo_ArrayLiteralNode* arrayLiteral = ALLOCATE_NODE(parser, ArrayLiteral, PICCOLO_EXPR_ARRAY_LITERAL);
        arrayLiteral->first = first;
        return (struct piccolo_ExprNode*)arrayLiteral;
    }
    return parseVar(PARSER_ARGS);
}

static struct piccolo_ExprNode* parseImport(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_IMPORT) {
        advanceParser(engine, parser);
        if(parser->currToken.type == PICCOLO_TOKEN_STRING) {
            if(parser->currToken.length > PICCOLO_MAX_PACKAGE) {
                parsingError(engine, parser, "Package import of length %d exceeded PICCOLO_MAX_PACKAGE (%d).", parser->currToken.length, PICCOLO_MAX_PACKAGE);
            }
            struct piccolo_Token packageName = parser->currToken;
            struct piccolo_ImportNode* import = ALLOCATE_NODE(parser, Import, PICCOLO_EXPR_IMPORT);
            import->packageName = packageName;
            advanceParser(engine, parser);
            if(parser->currToken.type == PICCOLO_TOKEN_AS) {
                advanceParser(engine, parser);
                struct piccolo_VarDeclNode* importAs = ALLOCATE_NODE(parser, VarDecl, PICCOLO_EXPR_VAR_DECL);
                importAs->name = parser->currToken;
                if(parser->currToken.type != PICCOLO_TOKEN_IDENTIFIER) {
                    parsingError(engine, parser, "Expected identifier.");
                }
                advanceParser(engine, parser);
                importAs->value = (struct piccolo_ExprNode*)import;
                importAs->mutable = false;
                return (struct piccolo_ExprNode*)importAs;
            }
            return (struct piccolo_ExprNode*)import;
        } else {
            parsingError(engine, parser, "Expected package name.");
            return NULL;
        }
    }
    return parseArrayLiteral(PARSER_ARGS);
}

static struct piccolo_ExprNode* parseFnLiteral(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_FN) {
        advanceParser(engine, parser);
        struct piccolo_FnLiteralNode* fnLiteral = ALLOCATE_NODE(parser, FnLiteral, PICCOLO_EXPR_FN_LITERAL);
        piccolo_initTokenArray(&fnLiteral->params);
        while(parser->currToken.type != PICCOLO_TOKEN_ARROW) {
            if(parser->currToken.type != PICCOLO_TOKEN_IDENTIFIER) {
                parsingError(engine, parser, "Expected ->.");
                break;
            } else {
                piccolo_writeTokenArray(engine, &fnLiteral->params, parser->currToken);
                advanceParser(engine, parser);
            }

            if(parser->currToken.type == PICCOLO_TOKEN_COMMA) {
                advanceParser(engine, parser);
                if(parser->currToken.type != PICCOLO_TOKEN_IDENTIFIER) {
                    parsingError(engine, parser, "Expected parameter name.");
                }
            } else if(parser->currToken.type != PICCOLO_TOKEN_ARROW) {
                parsingError(engine, parser, "Expected comma.");
            }
        }
        advanceParser(engine, parser);
        fnLiteral->value = parseExpr(PARSER_ARGS_REQ_VAL);
        return (struct piccolo_ExprNode*)fnLiteral;
    }
    return parseImport(PARSER_ARGS);
}

static struct piccolo_ExprNode* parseSubscript(PARSER_PARAMS) {
    SKIP_NEWLINES()
    struct piccolo_ExprNode* value = parseFnLiteral(PARSER_ARGS);
    while(parser->currToken.type == PICCOLO_TOKEN_DOT || parser->currToken.type == PICCOLO_TOKEN_LEFT_SQR_PAREN) {
        if(parser->currToken.type == PICCOLO_TOKEN_DOT) {

            advanceParser(engine, parser);
            if (parser->currToken.type == PICCOLO_TOKEN_IDENTIFIER) {
                struct piccolo_Token subscript = parser->currToken;
                advanceParser(engine, parser);
                if (parser->currToken.type == PICCOLO_TOKEN_EQ) {
                    advanceParser(engine, parser);
                    struct piccolo_SubscriptSetNode *subscriptSet = ALLOCATE_NODE(parser, SubscriptSet,
                                                                                  PICCOLO_EXPR_SUBSCRIPT_SET);
                    subscriptSet->target = value;
                    subscriptSet->subscript = subscript;
                    subscriptSet->value = parseExpr(PARSER_ARGS_REQ_VAL);
                    return (struct piccolo_ExprNode *) subscriptSet;
                } else {
                    struct piccolo_SubscriptNode *subscriptNode = ALLOCATE_NODE(parser, Subscript,
                                                                                PICCOLO_EXPR_SUBSCRIPT);
                    subscriptNode->value = value;
                    subscriptNode->subscript = subscript;
                    value = (struct piccolo_ExprNode *) subscriptNode;
                }
            } else {
                parsingError(engine, parser, "Expected name.");
            }

        } else {

            int charIdx = parser->currToken.charIdx;
            advanceParser(engine, parser);
            struct piccolo_ExprNode* index = parseExpr(PARSER_ARGS_REQ_VAL);

            if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_SQR_PAREN) {
                advanceParser(engine, parser);
            } else {
                parsingError(engine, parser, "Expected ].");
            }

            if(parser->currToken.type == PICCOLO_TOKEN_EQ) {

                advanceParser(engine, parser);

                struct piccolo_IndexSetNode* indexSet = ALLOCATE_NODE(parser, IndexSet, PICCOLO_EXPR_INDEX_SET);
                indexSet->charIdx = charIdx;
                indexSet->target = value;
                indexSet->index = index;
                indexSet->value = parseExpr(PARSER_ARGS_REQ_VAL);

                return (struct piccolo_ExprNode*)indexSet;

            } else {
                struct piccolo_IndexNode* indexNote = ALLOCATE_NODE(parser, Index, PICCOLO_EXPR_INDEX);
                indexNote->charIdx = charIdx;
                indexNote->target = value;
                indexNote->index = index;
                value = (struct piccolo_ExprNode*)indexNote;
            }

        }
    }
    return value;
}

static struct piccolo_ExprNode* parseCall(PARSER_PARAMS) {
    SKIP_NEWLINES()
    struct piccolo_ExprNode* function = parseSubscript(PARSER_ARGS);
    while(parser->currToken.type == PICCOLO_TOKEN_LEFT_PAREN) {
        int charIdx = parser->currToken.charIdx;
        advanceParser(engine, parser);
        struct piccolo_ExprNode* firstArg = NULL;
        struct piccolo_ExprNode* curr = NULL;
        while(parser->currToken.type != PICCOLO_TOKEN_RIGHT_PAREN) {
            if(parser->currToken.type == PICCOLO_TOKEN_EOF) {
                parsingError(engine, parser, "Expected ).");
                return NULL;
            }
            struct piccolo_ExprNode* arg = parseExpr(PARSER_ARGS_REQ_VAL);
            if(curr == NULL) {
                firstArg = arg;
                curr = arg;
            } else {
                curr->nextExpr = arg;
                curr = arg;
            }

            while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
                advanceParser(engine, parser);
            
            if(parser->currToken.type == PICCOLO_TOKEN_COMMA) {
                advanceParser(engine, parser);
                if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_PAREN) {
                    parsingError(engine, parser, "Expected argument.");
                    advanceParser(engine, parser);
                    return function;
                }
            } else if(parser->currToken.type == PICCOLO_TOKEN_RIGHT_PAREN) {

            } else {
                parsingError(engine, parser, "Expected comma.");
                return NULL;
            }
        }
        advanceParser(engine, parser);
        struct piccolo_CallNode* functionCall = ALLOCATE_NODE(parser, Call, PICCOLO_EXPR_CALL);
        functionCall->function = function;
        functionCall->firstArg = firstArg;
        functionCall->charIdx = charIdx;
        function = (struct piccolo_ExprNode*)functionCall;
    }
    return function;
}

static struct piccolo_ExprNode* parseUnary(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_MINUS ||
       parser->currToken.type == PICCOLO_TOKEN_BANG) {
        struct piccolo_Token op = parser->currToken;
        advanceParser(engine, parser);
        struct piccolo_ExprNode* value = parseUnary(PARSER_ARGS_REQ_VAL);
        struct piccolo_UnaryNode* unary = ALLOCATE_NODE(parser, Unary, PICCOLO_EXPR_UNARY);
        unary->op = op;
        unary->value = value;
        return (struct piccolo_ExprNode*)unary;
    }
    return parseCall(PARSER_ARGS);
}

static struct piccolo_ExprNode* parseRange(PARSER_PARAMS) {
    SKIP_NEWLINES()
    struct piccolo_ExprNode* left = parseUnary(PARSER_ARGS);
    if(parser->currToken.type == PICCOLO_TOKEN_DOT_DOT) {
        int charIdx = parser->currToken.charIdx;
        advanceParser(engine, parser);
        struct piccolo_ExprNode* right = parseRange(PARSER_ARGS);
        struct piccolo_RangeNode* range = ALLOCATE_NODE(parser, Range, PICCOLO_EXPR_RANGE);
        range->left = left;
        range->right = right;
        range->charIdx = charIdx;
        return (struct piccolo_ExprNode*)range;
    }
    return left;
}

static struct piccolo_ExprNode* parseMultiplicative(PARSER_PARAMS) {
    SKIP_NEWLINES()
    struct piccolo_ExprNode* expr = parseRange(PARSER_ARGS);
    while(parser->currToken.type == PICCOLO_TOKEN_STAR ||
          parser->currToken.type == PICCOLO_TOKEN_SLASH ||
          parser->currToken.type == PICCOLO_TOKEN_PERCENT) {
        struct piccolo_Token op = parser->currToken;
        advanceParser(engine, parser);
        struct piccolo_ExprNode* rightHand = parseRange(PARSER_ARGS_REQ_VAL);
        struct piccolo_BinaryNode* binary = ALLOCATE_NODE(parser, Binary, PICCOLO_EXPR_BINARY);
        binary->a = expr;
        binary->op = op;
        binary->b = rightHand;
        expr = (struct piccolo_ExprNode*)binary;
    }
    return expr;
}

static struct piccolo_ExprNode* parseAdditive(PARSER_PARAMS) {
    SKIP_NEWLINES()
    struct piccolo_ExprNode* expr = parseMultiplicative(PARSER_ARGS);
    while(parser->currToken.type == PICCOLO_TOKEN_PLUS ||
          parser->currToken.type == PICCOLO_TOKEN_MINUS) {
        struct piccolo_Token op = parser->currToken;
        advanceParser(engine, parser);
        struct piccolo_ExprNode* rightHand = parseMultiplicative(PARSER_ARGS_REQ_VAL);
        struct piccolo_BinaryNode* binary = ALLOCATE_NODE(parser, Binary, PICCOLO_EXPR_BINARY);
        binary->a = expr;
        binary->op = op;
        binary->b = rightHand;
        expr = (struct piccolo_ExprNode*)binary;
    }
    return expr;
}

static struct piccolo_ExprNode* parseIn(PARSER_PARAMS) {
    SKIP_NEWLINES()
    struct piccolo_ExprNode* expr = parseAdditive(PARSER_ARGS);
    while(parser->currToken.type == PICCOLO_TOKEN_IN) {
        struct piccolo_Token op = parser->currToken;
        advanceParser(engine, parser);
        struct piccolo_ExprNode* rightHand = parseAdditive(PARSER_ARGS_REQ_VAL);
        struct piccolo_BinaryNode* binary = ALLOCATE_NODE(parser, Binary, PICCOLO_EXPR_BINARY);
        binary->a = expr;
        binary->b = rightHand;
        binary->op = op;
        expr = (struct piccolo_ExprNode*)binary;
    }
    return expr;
}

static struct piccolo_ExprNode* parseComparison(PARSER_PARAMS) {
    SKIP_NEWLINES()
    struct piccolo_ExprNode* expr = parseIn(PARSER_ARGS);
    while(parser->currToken.type == PICCOLO_TOKEN_GREATER ||
          parser->currToken.type == PICCOLO_TOKEN_LESS ||
          parser->currToken.type == PICCOLO_TOKEN_GREATER_EQ ||
          parser->currToken.type == PICCOLO_TOKEN_LESS_EQ) {
        struct piccolo_Token op = parser->currToken;
        advanceParser(engine, parser);
        struct piccolo_ExprNode* rightHand = parseIn(PARSER_ARGS_REQ_VAL);
        struct piccolo_BinaryNode* binary = ALLOCATE_NODE(parser, Binary, PICCOLO_EXPR_BINARY);
        binary->a = expr;
        binary->op = op;
        binary->b = rightHand;
        expr = (struct piccolo_ExprNode*)binary;
    }
    return expr;
}

static struct piccolo_ExprNode* parseEquality(PARSER_PARAMS) {
    SKIP_NEWLINES()
    struct piccolo_ExprNode* expr = parseComparison(PARSER_ARGS);
    while(parser->currToken.type == PICCOLO_TOKEN_EQ_EQ ||
          parser->currToken.type == PICCOLO_TOKEN_BANG_EQ) {
        struct piccolo_Token op = parser->currToken;
        advanceParser(engine, parser);
        struct piccolo_ExprNode* rightHand = parseEquality(PARSER_ARGS_REQ_VAL);
        struct piccolo_BinaryNode* binary = ALLOCATE_NODE(parser, Binary, PICCOLO_EXPR_BINARY);
        binary->a = expr;
        binary->op = op;
        binary->b = rightHand;
        expr = (struct piccolo_ExprNode*)binary;
    }
    return expr;
}

static struct piccolo_ExprNode* parseBoolean(PARSER_PARAMS) {
    SKIP_NEWLINES()
    struct piccolo_ExprNode* expr = parseEquality(PARSER_ARGS);
    while(parser->currToken.type == PICCOLO_TOKEN_AND ||
          parser->currToken.type == PICCOLO_TOKEN_OR) {
        struct piccolo_Token op = parser->currToken;
        advanceParser(engine, parser);
        struct piccolo_ExprNode* rightHand = parseBoolean(PARSER_ARGS_REQ_VAL);
        struct piccolo_BinaryNode* binary = ALLOCATE_NODE(parser, Binary, PICCOLO_EXPR_BINARY);
        binary->a = expr;
        binary->op = op;
        binary->b = rightHand;
        expr = (struct piccolo_ExprNode*)binary;
    }
    return expr;
}

static struct piccolo_ExprNode* parseVarDecl(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_VAR || parser->currToken.type == PICCOLO_TOKEN_CONST) {
        struct piccolo_VarDeclNode* varDecl = ALLOCATE_NODE(parser, VarDecl, PICCOLO_EXPR_VAR_DECL);
        varDecl->mutable = parser->currToken.type == PICCOLO_TOKEN_VAR;
        advanceParser(engine, parser);
        if(parser->currToken.type == PICCOLO_TOKEN_IDENTIFIER) {
            varDecl->name = parser->currToken;
            advanceParser(engine, parser);
        } else {
            parsingError(engine, parser, "Expected variable name.");
        }

        if(parser->currToken.type == PICCOLO_TOKEN_EQ) {
            advanceParser(engine, parser);
        } else {
            parsingError(engine, parser, "Expected =.");
        }

        varDecl->value = parseExpr(PARSER_ARGS_REQ_VAL);

        return (struct piccolo_ExprNode*)varDecl;
    }
    return parseBoolean(PARSER_ARGS);
}

static struct piccolo_ExprNode* parseIf(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_IF) {
        advanceParser(engine, parser);
        int charIdx = parser->currToken.charIdx;
        struct piccolo_ExprNode* condition = parseExpr(PARSER_ARGS_REQ_VAL);
        struct piccolo_ExprNode* trueVal = parseExpr(PARSER_ARGS_REQ_VAL);
        struct piccolo_ExprNode* falseVal = NULL;
        while(parser->currToken.type == PICCOLO_TOKEN_NEWLINE)
            advanceParser(engine, parser);
        if(parser->currToken.type == PICCOLO_TOKEN_ELSE) {
            advanceParser(engine, parser);
            falseVal = parseExpr(PARSER_ARGS_REQ_VAL);
        }
        struct piccolo_IfNode* ifNode = ALLOCATE_NODE(parser, If, PICCOLO_EXPR_IF);
        ifNode->conditionCharIdx = charIdx;
        ifNode->condition = condition;
        ifNode->trueVal = trueVal;
        ifNode->falseVal = falseVal;
        return (struct piccolo_ExprNode*)ifNode;
    }
    return parseVarDecl(PARSER_ARGS);
}

static struct piccolo_ExprNode* parseWhile(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_WHILE) {
        advanceParser(engine, parser);
        struct piccolo_WhileNode* whileNode = ALLOCATE_NODE(parser, While, PICCOLO_EXPR_WHILE);
        whileNode->conditionCharIdx = parser->currToken.charIdx;
        whileNode->condition = parseExpr(PARSER_ARGS_REQ_VAL);
        whileNode->value = parseExpr(PARSER_ARGS_REQ_VAL);
        return (struct piccolo_ExprNode*)whileNode;
    }
    return parseIf(PARSER_ARGS);
}

static struct piccolo_ExprNode* parseFor(PARSER_PARAMS) {
    SKIP_NEWLINES()
    if(parser->currToken.type == PICCOLO_TOKEN_FOR) {
        advanceParser(engine, parser);
        struct piccolo_Token name = parser->currToken;
        if(parser->currToken.type != PICCOLO_TOKEN_IDENTIFIER) {
            parsingError(engine, parser, "Expected identifier.");
        }
        advanceParser(engine, parser);
        int charIdx = parser->currToken.charIdx;
        if(parser->currToken.type != PICCOLO_TOKEN_IN) {
            parsingError(engine, parser, "Expected in.");
        } else {
            advanceParser(engine, parser);
        }
        int containerCharIdx = parser->currToken.charIdx;
        struct piccolo_ExprNode* container = parseExpr(PARSER_ARGS_REQ_VAL);
        struct piccolo_ExprNode* value = parseExpr(PARSER_ARGS_REQ_VAL);

        struct piccolo_ForNode* forNode = ALLOCATE_NODE(parser, For, PICCOLO_EXPR_FOR);
        forNode->container = container;
        forNode->name = name;
        forNode->value = value;
        forNode->containerCharIdx = containerCharIdx;
        forNode->charIdx = charIdx;
        return (struct piccolo_ExprNode*)forNode;
    }
    return parseWhile(PARSER_ARGS);
}

static struct piccolo_ExprNode* parseExpr(PARSER_PARAMS) {
    SKIP_NEWLINES()
    return parseFor(PARSER_ARGS);
}

struct piccolo_ExprNode* piccolo_parse(struct piccolo_Engine* engine, struct piccolo_Parser* parser) {
    return parseExprList(engine, parser, false);
}

void piccolo_initParser(struct piccolo_Engine* engine, struct piccolo_Parser* parser, struct piccolo_Scanner* scanner, struct piccolo_Package* package) {
    parser->scanner = scanner;
    parser->nodes = NULL;
    parser->hadError = false;
    parser->cycled = false;
    parser->package = package;
    advanceParser(engine, parser);
}

void piccolo_freeParser(struct piccolo_Engine* engine, struct piccolo_Parser* parser) {
    struct piccolo_ExprNode* curr = parser->nodes;
    while(curr != NULL) {
        struct piccolo_ExprNode* next = curr->nodes;
        if(curr->type == PICCOLO_EXPR_FN_LITERAL) {
            struct piccolo_FnLiteralNode* fnLiteral = (struct piccolo_FnLiteralNode*)curr;
            piccolo_freeTokenArray(engine, &fnLiteral->params);
        }
        free(curr);
        curr = next;
    }
}


#ifndef PICCOLO_SCANNER_H
#define PICCOLO_SCANNER_H

#include <stddef.h>
#include <stdint.h>

enum piccolo_TokenType {
    // Single chars
    PICCOLO_TOKEN_PLUS, PICCOLO_TOKEN_MINUS, PICCOLO_TOKEN_STAR, PICCOLO_TOKEN_SLASH, PICCOLO_TOKEN_PERCENT,
    PICCOLO_TOKEN_COMMA,
    PICCOLO_TOKEN_EQ,
    PICCOLO_TOKEN_GREATER, PICCOLO_TOKEN_LESS,
    PICCOLO_TOKEN_BANG,
    PICCOLO_TOKEN_LEFT_PAREN, PICCOLO_TOKEN_RIGHT_PAREN,
    PICCOLO_TOKEN_LEFT_SQR_PAREN, PICCOLO_TOKEN_RIGHT_SQR_PAREN,
    PICCOLO_TOKEN_LEFT_BRACE, PICCOLO_TOKEN_RIGHT_BRACE,
    PICCOLO_TOKEN_DOT,

    // 2 chars
    PICCOLO_TOKEN_EQ_EQ,
    PICCOLO_TOKEN_ARROW,
    PICCOLO_TOKEN_GREATER_EQ, PICCOLO_TOKEN_LESS_EQ,
    PICCOLO_TOKEN_DOT_DOT,

    // Literals
    PICCOLO_TOKEN_NUM, PICCOLO_TOKEN_NIL, PICCOLO_TOKEN_TRUE, PICCOLO_TOKEN_FALSE, PICCOLO_TOKEN_STRING,

    // Keywords
    PICCOLO_TOKEN_VAR,
    PICCOLO_TOKEN_FN,
    PICCOLO_TOKEN_AND, PICCOLO_TOKEN_OR,
    PICCOLO_TOKEN_IF, PICCOLO_TOKEN_ELSE,
    PICCOLO_TOKEN_WHILE,
    PICCOLO_TOKEN_FOR, PICCOLO_TOKEN_IN,

    // Misc
    PICCOLO_TOKEN_IDENTIFIER,
    PICCOLO_TOKEN_ERROR,
    PICCOLO_TOKEN_EOF,
};

struct piccolo_Scanner {
    char* source;
    char* start;
    char* current;
};

struct piccolo_Token {
    char* start;
    uint32_t charIdx;
    size_t length;
    enum piccolo_TokenType type;
};

void piccolo_initScanner(struct piccolo_Scanner* scanner, char* source);
struct piccolo_Token piccolo_nextToken(struct piccolo_Scanner* scanner);

#endif

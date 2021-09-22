
#ifndef PICCOLO_SCANNER_H
#define PICCOLO_SCANNER_H

#include <stddef.h>
#include <stdint.h>

enum piccolo_TokenType {
    // Single chars
    PICCOLO_TOKEN_PLUS, PICCOLO_TOKEN_MINUS, PICCOLO_TOKEN_STAR, PICCOLO_TOKEN_SLASH,
    PICCOLO_TOKEN_LEFT_PAREN, PICCOLO_TOKEN_RIGHT_PAREN,

    // Literals
    PICCOLO_TOKEN_NUM, PICCOLO_TOKEN_NIL, PICCOLO_TOKEN_TRUE, PICCOLO_TOKEN_FALSE,

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
    const char* start;
    uint32_t charIdx;
    size_t length;
    enum piccolo_TokenType type;
};

void piccolo_initScanner(struct piccolo_Scanner* scanner, char* source);
struct piccolo_Token piccolo_nextToken(struct piccolo_Scanner* scanner);

#endif

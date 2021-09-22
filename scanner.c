
#include "scanner.h"

#include <string.h>
#include <stdbool.h>

void piccolo_initScanner(struct piccolo_Scanner* scanner, char* source) {
    scanner->source = source;
    scanner->start = source;
    scanner->current = source;
}

static void skipWhitespace(struct piccolo_Scanner* scanner) {
    while(*scanner->current == ' ' || *scanner->current == '\t' || *scanner->current == '\n') {
        scanner->current++;
    }
    scanner->start = scanner->current;
}

static bool numeric(char c) {
    return c >= '0' && c <= '9';
}

static bool alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool alphanumeric(char c) {
    return alpha(c) || numeric(c);
}

static enum piccolo_TokenType getKeyword(char* start, char* end) {
    #define TOKEN_TYPE(token, keyword)                                                        \
        if(end - start < sizeof keyword && memcmp(start, keyword, (sizeof keyword) - 1) == 0) \
            return PICCOLO_TOKEN_ ## token;

    TOKEN_TYPE(NIL, "nil")
    TOKEN_TYPE(TRUE, "true")
    TOKEN_TYPE(FALSE, "false")
    return PICCOLO_TOKEN_IDENTIFIER;
    #undef TOKEN_TYPE
}

static struct piccolo_Token makeToken(struct piccolo_Scanner* scanner, enum piccolo_TokenType type) {
    struct piccolo_Token token;
    token.start = scanner->start;
    token.length = scanner->current - scanner->start;
    token.type = type;
    token.charIdx = scanner->start - scanner->source;
    scanner->start = scanner->current;
    return token;
}

struct piccolo_Token piccolo_nextToken(struct piccolo_Scanner* scanner) {
    skipWhitespace(scanner);

    switch(*scanner->current) {
        case '+': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_PLUS);
        }
        case '-': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_MINUS);
        }
        case '*': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_STAR);
        }
        case '/': {
            scanner->current++;
            if(*scanner->current == '/') { // Comment
                while(*scanner->current != '\n' && *scanner->current != '\0')
                    scanner->current++;
                scanner->start = scanner->current;
                return piccolo_nextToken(scanner);
            }
            return makeToken(scanner, PICCOLO_TOKEN_SLASH);
        }
        case '(': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_LEFT_PAREN);
        }
        case ')': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_RIGHT_PAREN);
        }
        case '\0': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_EOF);
        }
    }

    if(numeric(*scanner->start)) {
        while(numeric(*scanner->current)) {
            scanner->current++;
        }
        if(*scanner->current == '.')
            scanner->current++;
        while(numeric(*scanner->current)) {
            scanner->current++;
        }
        return makeToken(scanner, PICCOLO_TOKEN_NUM);
    }

    if(alpha(*scanner->start)) {
        while(alphanumeric(*scanner->current)) {
            scanner->current++;
        }
        return makeToken(scanner, getKeyword(scanner->start, scanner->current));
    }

    scanner->current++;
    return makeToken(scanner, PICCOLO_TOKEN_ERROR);
}
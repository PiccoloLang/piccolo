
#include "scanner.h"
#include <string.h>
#include <stdbool.h>

void piccolo_initScanner(struct piccolo_Scanner* scanner, const char* source) {
    scanner->source = source;
    scanner->start = source;
    scanner->current = source;
}

static void skipWhitespace(struct piccolo_Scanner* scanner) {
    while(*scanner->current == ' ' || *scanner->current == '\t' || *scanner->current == '\r') {
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

static enum piccolo_TokenType getKeyword(const char* start, const char* end) {
    #define TOKEN_TYPE(token, keyword)                                                        \
        if(end - start == strlen(keyword) && memcmp(start, keyword, strlen(keyword)) == 0) \
            return PICCOLO_TOKEN_ ## token;
    #define MIN(a, b) ((a) < (b) ? (a) : (b))

    TOKEN_TYPE(NIL, "nil")
    TOKEN_TYPE(TRUE, "true")
    TOKEN_TYPE(FALSE, "false")

    TOKEN_TYPE(VAR, "var")
    TOKEN_TYPE(CONST, "const")
    TOKEN_TYPE(FN, "fn")
    TOKEN_TYPE(AND, "and")
    TOKEN_TYPE(OR, "or")
    TOKEN_TYPE(IF, "if")
    TOKEN_TYPE(ELSE, "else")
    TOKEN_TYPE(WHILE, "while")
    TOKEN_TYPE(FOR, "for")
    TOKEN_TYPE(IN, "in")
    TOKEN_TYPE(IMPORT, "import")
    TOKEN_TYPE(AS, "as")

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
            if(*scanner->current == '>') {
                scanner->current++;
                return makeToken(scanner, PICCOLO_TOKEN_ARROW);
            }
            return makeToken(scanner, PICCOLO_TOKEN_MINUS);
        }
        case '*': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_STAR);
        }
        case ',': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_COMMA);
        }
        case '/': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_SLASH);
        }
        case '%': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_PERCENT);
        }
        case '=': {
            scanner->current++;
            if(*scanner->current == '=') {
                scanner->current++;
                return makeToken(scanner, PICCOLO_TOKEN_EQ_EQ);
            }
            return makeToken(scanner, PICCOLO_TOKEN_EQ);
        }
        case '>': {
            scanner->current++;
            if(*scanner->current == '=') {
                scanner->current++;
                return makeToken(scanner, PICCOLO_TOKEN_GREATER_EQ);
            }
            return makeToken(scanner, PICCOLO_TOKEN_GREATER);
        }
        case '<': {
            scanner->current++;
            if(*scanner->current == '=') {
                scanner->current++;
                return makeToken(scanner, PICCOLO_TOKEN_LESS_EQ);
            }
            return makeToken(scanner, PICCOLO_TOKEN_LESS);
        }
        case '!': {
            scanner->current++;
            if(*scanner->current == '=') {
                scanner->current++;
                return makeToken(scanner, PICCOLO_TOKEN_BANG_EQ);
            }
            return makeToken(scanner, PICCOLO_TOKEN_BANG);
        }
        case '(': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_LEFT_PAREN);
        }
        case ')': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_RIGHT_PAREN);
        }
        case '[': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_LEFT_SQR_PAREN);
        }
        case ']': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_RIGHT_SQR_PAREN);
        }
        case '{': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_LEFT_BRACE);
        }
        case '}': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_RIGHT_BRACE);
        }
        case '\'': {
            scanner->current++;
            while(*scanner->current != '\'') {
                if(*scanner->current == '\0') {
                    scanner->source = scanner->current;
                    return makeToken(scanner, PICCOLO_TOKEN_ERROR);
                }
                scanner->current++;
            }
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_STRING);
        }
        case '.': {
            scanner->current++;
            if(*scanner->current == '.') {
                scanner->current++;
                return makeToken(scanner, PICCOLO_TOKEN_DOT_DOT);
            }
            return makeToken(scanner, PICCOLO_TOKEN_DOT);
        }
        case '#': {
            while(*scanner->current != '\n' && *scanner->current != '\0')
                scanner->current++;
            scanner->start = scanner->current;
            return piccolo_nextToken(scanner);
        }
        case ':': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_COLON);
        }
        case '\n': {
            scanner->current++;
            return makeToken(scanner, PICCOLO_TOKEN_NEWLINE);
        }
        case '\0': {
            return makeToken(scanner, PICCOLO_TOKEN_EOF);
        }
    }

    if(numeric(*scanner->start)) {
        while(numeric(*scanner->current)) {
            scanner->current++;
        }

        bool hadPeriod = *scanner->current == '.';
        if(*scanner->current == '.')
            scanner->current++;
        if(*scanner->current == '.') {
            scanner->current--;
            return makeToken(scanner, PICCOLO_TOKEN_NUM);
        }

        if(hadPeriod && !numeric(*scanner->current)) {
            scanner->current--;
        } else {
            while (numeric(*scanner->current)) {
                scanner->current++;
            }
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

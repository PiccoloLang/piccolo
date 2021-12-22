
#ifndef PICCOLO_STRUTIL_H
#define PICCOLO_STRUTIL_H

#include <stdint.h>

struct piccolo_strutil_LineInfo {
    // These are assigned with const values in several places and so should be marked const
    const char* lineStart;
    const char* lineEnd;
    int line;
};

struct piccolo_strutil_LineInfo piccolo_strutil_getLine(const char* source, uint32_t charIdx);
int piccolo_strutil_utf8Chars(char c);

#endif

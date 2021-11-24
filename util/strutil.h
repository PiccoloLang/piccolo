
#ifndef PICCOLO_STRUTIL_H
#define PICCOLO_STRUTIL_H

#include <stdint.h>

struct piccolo_strutil_LineInfo {
    char* lineStart;
    char* lineEnd;
    int line;
};

struct piccolo_strutil_LineInfo piccolo_strutil_getLine(const char* source, uint32_t charIdx);

#endif

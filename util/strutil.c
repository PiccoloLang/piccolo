
#include "strutil.h"

struct piccolo_strutil_LineInfo piccolo_strutil_getLine(const char* source, uint32_t charIdx) {
    struct piccolo_strutil_LineInfo lineInfo;
    if(!source) return (struct piccolo_strutil_LineInfo) {0};
    lineInfo.lineStart = source;
    lineInfo.line = 0;
    for(uint32_t i = 0; i < charIdx; i++) {
        if(source[i] == '\0') {
            break;
        }
        if(source[i] == '\n') {
            lineInfo.line++;
            lineInfo.lineStart = source + i + 1;
        }
    }
    lineInfo.lineEnd = lineInfo.lineStart;
    while(*lineInfo.lineEnd != '\0' && *lineInfo.lineEnd != '\n')
        lineInfo.lineEnd++;
    return lineInfo;
}

int piccolo_strutil_utf8Chars(char c) {
    if((c & 0xF8) == 0xF0)
        return 4;
    if((c & 0xF0) == 0xE0)
        return 3;
    if((c & 0xE0) == 0xC0)
        return 2;
    return 1;
}

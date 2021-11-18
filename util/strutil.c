
#include "strutil.h"

struct piccolo_strutil_LineInfo piccolo_strutil_getLine(const char* source, uint32_t charIdx) {
    struct piccolo_strutil_LineInfo lineInfo;
    lineInfo.lineStart = source;
    lineInfo.line = 0;
    for(uint32_t i = 0; i < charIdx; i++) {
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

#include <stdio.h>
#include <stdlib.h>

#include "file.h"

char* piccolo_readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if(file == NULL)
        return NULL;

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if(buffer == NULL)
        return NULL;
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if(bytesRead < fileSize)
        return NULL;
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}
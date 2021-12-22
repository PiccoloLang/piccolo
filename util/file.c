
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"

char* piccolo_readFile(const char* path) {
    FILE* file = fopen(path, "r");
    if(file == NULL)
        return NULL;

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    /* need to calloc to guarantee the appending of the null term */
    char* buffer = (char*)calloc(fileSize + 1, sizeof(char));
    if(buffer == NULL)
        return NULL;
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if(bytesRead < fileSize)
        return NULL;

    fclose(file);
    return buffer;
}

void piccolo_applyRelativePathToFilePath(char* dest, const char* relativePath, size_t relPathLen, const char* filepath) {
    strcpy(dest, filepath);
    int i = 0;
    int topNamed = 0;
    while(filepath[i] != 0) {
        if(filepath[i] == '/' && (i < 2 || !(filepath[i - 1] == '.' && filepath[i - 2] == '.'))) {
            topNamed++;
        }
        i++;
    }
    size_t currLen = strlen(dest);
    while(currLen > 0 && filepath[currLen - 1] != '/') {
        dest[currLen - 1] = 0;
        currLen--;
    }

    size_t start = 0;
    for(i = 0; i <= relPathLen; i++) {
        if (i == relPathLen || relativePath[i] == '/') {
            size_t len = i - start;
            if (len == 2 && relativePath[start] == '.' && relativePath[start + 1] == '.') {
                if (topNamed == 0) {
                    dest[currLen] = '.';
                    dest[currLen + 1] = '.';
                    dest[currLen + 2] = '/';
                    currLen += 3;
                    topNamed = 0;
                } else {
                    currLen--;
                    dest[currLen] = 0;
                    while(currLen > 0 && filepath[currLen - 1] != '/') {
                        dest[currLen - 1] = 0;
                        currLen--;
                    }
                    topNamed--;
                }
            } else {
                for (int j = start; j < i; j++) {
                    dest[currLen] = relativePath[j];
                    currLen++;
                    dest[currLen] = 0;
                }
                topNamed++;
                if (i != relPathLen) {
                    dest[currLen] = '/';
                    currLen++;
                    dest[currLen] = 0;
                }
            }
            start = i + 1;
        }
    }
}

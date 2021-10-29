
#ifndef PICCOLO_FILE_H
#define PICCOLO_FILE_H

char* piccolo_readFile(const char* path);

void piccolo_applyRelativePathToFilePath(char* dest, const char* relativePath, size_t relPathLen, const char* filepath);

#endif


#include "memory.h"
#include <stdlib.h>

void* reallocate(struct piccolo_Engine* engine, void* data, size_t oldSize, size_t newSize) {
    if(newSize == 0) {
        free(data);
        return NULL;
    }

    return realloc(data, newSize);
}
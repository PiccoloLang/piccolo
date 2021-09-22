
#ifndef PICCOLO_MEMORY_H
#define PICCOLO_MEMORY_H

#include <stddef.h>

struct piccolo_Engine;

void* reallocate(struct piccolo_Engine* engine, void* data, size_t oldSize, size_t newSize);

#endif

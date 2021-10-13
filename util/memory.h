
#ifndef PICCOLO_MEMORY_H
#define PICCOLO_MEMORY_H

#include <stddef.h>

struct piccolo_Engine;

#define PICCOLO_ENABLE_MEMORY_TRACKER

#ifdef PICCOLO_ENABLE_MEMORY_TRACKER

#define PICCOLO_REALLOCATE(allocName, engine, data, oldSize, newSize) \
        piccolo_reallocateTrack(__FILE__ ":" allocName, engine, data, oldSize, newSize)

struct piccolo_MemoryTrack {
    void* ptr;
    size_t size;
    const char* name;
    struct piccolo_MemoryTrack* next;
};

void* piccolo_reallocateTrack(const char* name, struct piccolo_Engine* engine, void* data, size_t oldSize, size_t newSize);
void piccolo_printMemAllocs(struct piccolo_Engine* engine);
#else
#define PICCOLO_REALLOCATE(allocName, engine, data, oldSize, newSize) \
        piccolo_reallocate(engine, data, oldSize, newSize)
#endif

void* piccolo_reallocate(struct piccolo_Engine* engine, void* data, size_t oldSize, size_t newSize);

#endif

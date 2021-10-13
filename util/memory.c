
#include "memory.h"
#include "../engine.h"
#include <stdlib.h>

void* piccolo_reallocate(struct piccolo_Engine* engine, void* data, size_t oldSize, size_t newSize) {
    if(newSize == 0) {
        free(data);
        return NULL;
    }

    return realloc(data, newSize);
}

#ifdef PICCOLO_ENABLE_MEMORY_TRACKER

#include <stdio.h>

void* piccolo_reallocateTrack(const char* name, struct piccolo_Engine* engine, void* data, size_t oldSize, size_t newSize) {
    struct piccolo_MemoryTrack* track = engine->track;
    while(track != NULL) {
        if(track->ptr == data) {
            break;
        }
        track = track->next;
    }
    if(track == NULL) {
        track = malloc(sizeof(struct piccolo_MemoryTrack));
        track->next = engine->track;
        engine->track = track;
    }
    void* newPtr = piccolo_reallocate(engine, data, oldSize, newSize);
    track->ptr = newPtr;
    track->name = name;
    track->size = newSize;
    return newPtr;
}

void piccolo_printMemAllocs(struct piccolo_Engine* engine) {
    struct piccolo_MemoryTrack* track = engine->track;
    while(track != NULL) {
        if(track->size > 0) {
            printf("%s\n", track->name);
            printf("\tsize: %lu\n", track->size);
        }
        track = track->next;
    }
}

#endif
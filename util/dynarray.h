
#ifndef PICCOLO_DYNARRAY_H
#define PICCOLO_DYNARRAY_H

#include "memory.h"

struct piccolo_Engine;

#define PICCOLO_ARRAY_NAME(typename) piccolo_ ## typename ## Array
#define PICCOLO_GROW_CAPACITY(oldCapacity) ((oldCapacity < 8) ? 8 : 2 * oldCapacity)
#define PICCOLO_GROW_ARRAY(engine, type, pointer, oldCount, newCount) \
    ((type*)PICCOLO_REALLOCATE("array grow", engine, pointer, sizeof(type) * oldCount, sizeof(type) * newCount))
#define PICCOLO_FREE_ARRAY(engine, type, pointer, oldCount) \
    ((type*)PICCOLO_REALLOCATE("array free", engine, pointer, sizeof(type)* oldCount, 0))

#define PICCOLO_DYNARRAY_HEADER(type, typename)                                                 \
    struct PICCOLO_ARRAY_NAME(typename) {                                                       \
        int count;                                                                              \
        int capacity;                                                                           \
        type* values;                                                                           \
    };                                                                                          \
                                                                                                \
    void piccolo_init ## typename ## Array(struct PICCOLO_ARRAY_NAME(typename)* array);         \
    void piccolo_write ## typename ## Array(struct piccolo_Engine* engine, struct PICCOLO_ARRAY_NAME(typename)* array, type value); \
    void piccolo_free ## typename ## Array(struct piccolo_Engine* engine, struct PICCOLO_ARRAY_NAME(typename)* array);              \
    type piccolo_pop ## typename ## Array(struct PICCOLO_ARRAY_NAME(typename)* array);

#define PICCOLO_DYNARRAY_IMPL(type, typename)                                                   \
    void piccolo_init ## typename ## Array(struct PICCOLO_ARRAY_NAME(typename)* array) {        \
        array->count = 0;                                                                       \
        array->capacity = 0;                                                                    \
        array->values = NULL;                                                                   \
    }                                                                                           \
                                                                                                \
    void piccolo_write ## typename ## Array(struct piccolo_Engine* engine, struct PICCOLO_ARRAY_NAME(typename)* array, type value) { \
        if(array->capacity < array->count + 1) {                                                \
            int oldCapacity = array->capacity;                                                  \
            array->capacity = PICCOLO_GROW_CAPACITY(oldCapacity);                               \
            array->values = PICCOLO_GROW_ARRAY(engine, type, array->values, oldCapacity, array->capacity); \
        }                                                                                       \
        array->values[array->count] = value;                                                    \
        array->count++;                                                                         \
    }                                                                                           \
                                                                                                \
    void piccolo_free ## typename ## Array(struct piccolo_Engine* engine, struct PICCOLO_ARRAY_NAME(typename)* array) { \
        array->values = PICCOLO_FREE_ARRAY(engine, type, array->values, array->capacity);       \
    }                                                                                           \
                                                                                                \
    type piccolo_pop ## typename ## Array(struct PICCOLO_ARRAY_NAME(typename)* array) {         \
        array->count--;                                                                         \
        return array->values[array->count];                                                     \
    }

#endif

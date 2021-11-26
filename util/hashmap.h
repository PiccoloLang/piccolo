
#ifndef PICCOLO_HASHMAP_H
#define PICCOLO_HASHMAP_H

#include "dynarray.h"

#define PICCOLO_MAX_LOAD_FACTOR 0.75

#define PICCOLO_ALLOCATE(engine, type, count) ((type*)PICCOLO_REALLOCATE("hashmap alloc", engine, NULL, 0, sizeof(type) * count))

#define PICCOLO_HASHMAP_HEADER(keyType, valType, name) \
    struct piccolo_ ## name ## Entry {                 \
        keyType key;                                   \
        valType val;                                   \
    };                                                 \
    \
    struct piccolo_ ## name {                          \
        int count;                                     \
        int capacity;                                  \
        struct piccolo_ ## name ## Entry* entries;            \
        \
    };                                                 \
                                                       \
    void piccolo_init ## name(struct piccolo_ ## name * hashmap); \
    void piccolo_free ## name(struct piccolo_Engine* engine, struct piccolo_ ## name * hashmap); \
    void piccolo_set ## name(struct piccolo_Engine* engine, struct piccolo_ ## name * hashmap, keyType key, valType val); \
    valType piccolo_get ## name(struct piccolo_Engine* engine, struct piccolo_ ## name * hashmap, keyType key);                                                 \
    uint32_t piccolo_hash ## name ## Key(keyType key); \
    bool piccolo_compare ## name ## Keys(keyType a, keyType b); \
    bool piccolo_ ## name ## IsBaseKey(keyType key);

#define PICCOLO_HASHMAP_IMPL(keyType, valType, name, baseKey, baseVal) \
    void piccolo_init ## name(struct piccolo_ ## name * hashmap) { \
        hashmap->count = 0;                          \
        hashmap->capacity = 0;                       \
        hashmap->entries = NULL;\
    }                                                \
                                                     \
    void piccolo_free ## name(struct piccolo_Engine* engine, struct piccolo_ ## name * hashmap) { \
        hashmap->entries = PICCOLO_FREE_ARRAY(engine, struct piccolo_ ## name ## Entry, hashmap->entries, hashmap->capacity); \
        piccolo_init ## name(hashmap);\
    }                                                \
                                                     \
    static struct piccolo_ ## name ## Entry* find ## name ## Entry(struct piccolo_ ## name ## Entry * entries, int capacity, keyType key) {        \
        uint32_t index = piccolo_hash ## name ## Key(key) & (capacity - 1);                             \
        for(;;) {                                    \
            struct piccolo_ ## name ## Entry* entry = &entries[index];                            \
            if(piccolo_ ## name ## IsBaseKey(entry->key) || piccolo_compare ## name ## Keys(key, entry->key)) {     \
                return entry;                                                    \
            }                                                   \
            index = (index + 1) & (capacity - 1);\
        }\
    }                                                         \
                                                              \
    static void adjust ## name ## Capacity(struct piccolo_Engine* engine, struct piccolo_ ## name * hashmap, int capacity) {  \
        struct piccolo_ ## name ## Entry* entries = PICCOLO_ALLOCATE(engine, struct piccolo_ ## name ## Entry, capacity);                   \
        for(int i = 0; i < capacity; i++) {                   \
            entries[i].key = baseKey;                                   \
            entries[i].val = (baseVal); \
        }                                                              \
                                                                       \
        for(int i = 0; i < hashmap->capacity; i++) {                     \
            struct piccolo_ ## name ## Entry* entry = &hashmap->entries[i];                         \
            if(piccolo_ ## name ## IsBaseKey(entry->key)) continue;                        \
                                                                       \
            struct piccolo_ ## name ## Entry* dest = find ## name ## Entry(entries, capacity, entry->key);                    \
            dest->key = entry->key;                                    \
            dest->val = entry->val;\
        }                                                              \
                                                                       \
        PICCOLO_FREE_ARRAY(engine, struct piccolo_ ## name ## Entry, hashmap->entries, hashmap->capacity);\
                                                                       \
        hashmap->entries = entries;                                    \
        hashmap->capacity = capacity;\
    }\
                                                   \
    void piccolo_set ## name(struct piccolo_Engine* engine, struct piccolo_ ## name * hashmap, keyType key, valType val) {    \
        if(hashmap->count + 1 > hashmap->capacity * PICCOLO_MAX_LOAD_FACTOR) {                    \
            int capacity = PICCOLO_GROW_CAPACITY(hashmap->capacity);       \
            adjust ## name ## Capacity(engine, hashmap, capacity);\
        }\
        struct piccolo_ ## name ## Entry* entry = find ## name ## Entry(hashmap->entries, hashmap->capacity, key);    \
        if(piccolo_ ## name ## IsBaseKey(entry->key))                                      \
            hashmap->count++;\
        entry->key = key;                                       \
        entry->val = val;\
    }                                                                  \
                                                                       \
    valType piccolo_get ## name(struct piccolo_Engine* engine, struct piccolo_ ## name * hashmap, keyType key) {                 \
        if(hashmap->count == 0)                                        \
            return (baseVal);                           \
        struct piccolo_ ## name ## Entry* entry = find ## name ## Entry(hashmap->entries, hashmap->capacity, key);            \
        if(piccolo_ ## name ## IsBaseKey(entry->key))                                      \
            return (baseVal);                                               \
        return entry->val;\
    }\

#endif

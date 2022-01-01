
#ifndef PICCOLO_OBJECT_H
#define PICCOLO_OBJECT_H

#include "value.h"
#include "bytecode.h"

enum piccolo_ObjType {
    PICCOLO_OBJ_STRING,
    PICCOLO_OBJ_ARRAY,
    PICCOLO_OBJ_HASHMAP,
    PICCOLO_OBJ_FUNC,
    PICCOLO_OBJ_UPVAL,
    PICCOLO_OBJ_CLOSURE,
    PICCOLO_OBJ_NATIVE_FN,
    PICCOLO_OBJ_NATIVE_STRUCT,
    PICCOLO_OBJ_PACKAGE,
};

struct piccolo_Obj {
    enum piccolo_ObjType type;
    struct piccolo_Obj* next;
    bool marked;
    bool printed;
};

struct piccolo_ObjString {
    struct piccolo_Obj obj;
    char* string;
    int len; // Number of bytes in memory(excluding null)
    int utf8Len; // Number of UTF8 chars
    uint32_t hash;
};

struct piccolo_ObjArray {
    struct piccolo_Obj obj;
    struct piccolo_ValueArray array;
};

#include "util/hashmap.h"
struct piccolo_HashmapValue {
    piccolo_Value value;
    bool exists;
};

PICCOLO_HASHMAP_HEADER(piccolo_Value, struct piccolo_HashmapValue, Hashmap)

struct piccolo_ObjHashmap {
    struct piccolo_Obj obj;
    struct piccolo_Hashmap hashmap;
};

struct piccolo_ObjFunction {
    struct piccolo_Obj obj;
    struct piccolo_Bytecode bytecode;
    int arity;
};

struct piccolo_ObjUpval {
    struct piccolo_Obj obj;
    union {
        piccolo_Value *ptr;
        int idx;
    } val;
    bool open;
    struct piccolo_ObjUpval* next;
};

struct piccolo_Package;

struct piccolo_ObjClosure {
    struct piccolo_Obj obj;
    struct piccolo_ObjUpval** upvals;
    int upvalCnt;
    struct piccolo_ObjFunction* prototype;
    struct piccolo_Package* package;
};

struct piccolo_ObjNativeFn {
    struct piccolo_Obj obj;
    piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, piccolo_Value self);
    piccolo_Value self;
};

struct piccolo_ObjNativeStruct {
    struct piccolo_Obj obj;
    void (*free)(void* payload);
    void (*gcMark)(void* payload);
    piccolo_Value (*index)(void* payload, struct piccolo_Engine* engine, piccolo_Value key, bool set, piccolo_Value value);
    const char* typename;
    size_t payloadSize;
};

struct piccolo_Obj* allocateObj(struct piccolo_Engine* engine, enum piccolo_ObjType type, size_t size);
#define PICCOLO_ALLOCATE_OBJ(engine, type, objType) ((type*)allocateObj(engine, objType, sizeof(type)))

struct piccolo_ObjNativeStruct* piccolo_allocNativeStruct(struct piccolo_Engine* engine, size_t size, const char* name);

#define PICCOLO_ALLOCATE_NATIVE_STRUCT(engine, type, name) ((struct piccolo_Obj*)piccolo_allocNativeStruct(engine, sizeof(type), name))
#define PICCOLO_GET_PAYLOAD(obj, type) ((type*)((void*)obj + sizeof(struct piccolo_ObjNativeStruct)))

void piccolo_freeObj(struct piccolo_Engine* engine, struct piccolo_Obj* obj);

struct piccolo_ObjString* piccolo_takeString(struct piccolo_Engine* engine, char* string);
struct piccolo_ObjString* piccolo_copyString(struct piccolo_Engine* engine, const char* string, int len);
struct piccolo_ObjArray* piccolo_newArray(struct piccolo_Engine* engine, int len);
struct piccolo_ObjHashmap* piccolo_newHashmap(struct piccolo_Engine* engine);
struct piccolo_ObjFunction* piccolo_newFunction(struct piccolo_Engine* engine);
struct piccolo_ObjUpval* piccolo_newUpval(struct piccolo_Engine* engine, int idx);
struct piccolo_ObjClosure* piccolo_newClosure(struct piccolo_Engine* engine, struct piccolo_ObjFunction* function, int upvals);
struct piccolo_ObjNativeFn* piccolo_makeNative(struct piccolo_Engine* engine, piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, piccolo_Value self));
struct piccolo_ObjNativeFn* piccolo_makeBoundNative(struct piccolo_Engine* engine, piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args, piccolo_Value self), piccolo_Value self);
struct piccolo_Package* piccolo_newPackage(struct piccolo_Engine* engine);

#endif


#ifndef PICCOLO_OBJECT_H
#define PICCOLO_OBJECT_H

#include "bytecode.h"
#include "engine.h"

enum piccolo_ObjType {
    PICCOLO_OBJ_STRING,
    PICCOLO_OBJ_ARRAY,
    PICCOLO_OBJ_FUNC,
    PICCOLO_OBJ_UPVAL,
    PICCOLO_OBJ_CLOSURE,
    PICCOLO_OBJ_NATIVE_FN
};

struct piccolo_Obj {
    enum piccolo_ObjType type;
};

struct piccolo_ObjString {
    struct piccolo_Obj obj;
    const char* string;
    int len;
};

struct piccolo_ObjArray {
    struct piccolo_Obj obj;
    struct piccolo_ValueArray array;
};

struct piccolo_ObjFunction {
    struct piccolo_Obj obj;
    struct piccolo_Bytecode bytecode;
    int arity;
};

struct piccolo_ObjUpval {
    struct piccolo_Obj obj;
    piccolo_Value* valPtr;
    bool open;
    struct piccolo_ObjUpval* next;
};

struct piccolo_ObjClosure {
    struct piccolo_Obj obj;
    struct piccolo_ObjUpval** upvals;
    struct piccolo_ObjFunction* prototype;
};

struct piccolo_ObjNativeFn {
    struct piccolo_Obj obj;
    piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args);
};

struct piccolo_ObjString* piccolo_takeString(struct piccolo_Engine* engine, const char* string);
struct piccolo_ObjString* piccolo_copyString(struct piccolo_Engine* engine, const char* string, int len);
struct piccolo_ObjArray* piccolo_newArray(struct piccolo_Engine* engine, int len);
struct piccolo_ObjFunction* piccolo_newFunction(struct piccolo_Engine* engine);
struct piccolo_ObjUpval* piccolo_newUpval(struct piccolo_Engine* engine, piccolo_Value* ptr);
struct piccolo_ObjClosure* piccolo_newClosure(struct piccolo_Engine* engine, struct piccolo_ObjFunction* function, int upvals);
struct piccolo_ObjNativeFn* piccolo_makeNative(struct piccolo_Engine* engine, piccolo_Value (*native)(struct piccolo_Engine* engine, int argc, struct piccolo_Value* args));

#endif

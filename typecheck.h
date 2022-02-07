
#ifndef PICCOLO_TYPECHECK_H
#define PICCOLO_TYPECHECK_H

#include "util/dynarray.h"

enum piccolo_TypeType {
    PICCOLO_TYPE_ANY,
    PICCOLO_TYPE_NUM,
    PICCOLO_TYPE_STR,
    PICCOLO_TYPE_BOOL,
    PICCOLO_TYPE_NIL,

    PICCOLO_TYPE_ARRAY,
    PICCOLO_TYPE_HASHMAP,

    PICCOLO_TYPE_FN,

    PICCOLO_TYPE_PKG,

    PICCOLO_TYPE_UNION
};

PICCOLO_DYNARRAY_HEADER(struct piccolo_Type*, Type)

struct piccolo_Type {
    enum piccolo_TypeType type;
    union {
        struct piccolo_Type* listElem;
        struct {
            struct piccolo_Type* key;
            struct piccolo_Type* val;
        } hashmap;
        struct {
            struct piccolo_Type* result;
            struct piccolo_TypeArray params;
        } fn;
        struct {
            struct piccolo_TypeArray types;
        } unionTypes;
        struct piccolo_Package* pkg;
    } subtypes;
    struct piccolo_Type* next;
};

void piccolo_freeType(struct piccolo_Engine* engine, struct piccolo_Type* type);
void piccolo_getTypename(struct piccolo_Type* type, char* dest);

struct piccolo_Compiler;
struct piccolo_ExprNode;

struct piccolo_Type* piccolo_simpleType(struct piccolo_Engine* engine, enum piccolo_TypeType type);
struct piccolo_Type* piccolo_arrayType(struct piccolo_Engine* engine, struct piccolo_Type* elemType);
struct piccolo_Type* piccolo_hashmapType(struct piccolo_Engine* engine, struct piccolo_Type* keyType, struct piccolo_Type* valType);
struct piccolo_Type* piccolo_fnType(struct piccolo_Engine* engine, struct piccolo_TypeArray* params, struct piccolo_Type* res);
struct piccolo_Type* piccolo_pkgType(struct piccolo_Engine* engine, struct piccolo_Package* pkg);
struct piccolo_Type* piccolo_unionType(struct piccolo_Engine* engine, struct piccolo_TypeArray* types);

struct piccolo_Type* piccolo_getType(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_ExprNode* expr);

#endif
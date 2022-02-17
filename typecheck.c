
#include "typecheck.h"
#include "compiler.h"
#include "package.h"
#include "parser.h"
#include "object.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

PICCOLO_DYNARRAY_IMPL(struct piccolo_Type*, Type)

void piccolo_getTypename(struct piccolo_Type* type, char* dest) {
    char* originalDest = dest;
    if(type == NULL) {
        sprintf(dest, "NULL");
        return;
    }
    switch(type->type) {
        case PICCOLO_TYPE_ANY: {
            sprintf(dest, "any");
            break;
        }
        case PICCOLO_TYPE_NUM: {
            sprintf(dest, "num");
            break;
        }
        case PICCOLO_TYPE_STR: {
            sprintf(dest, "str");
            break;
        }
        case PICCOLO_TYPE_BOOL: {
            sprintf(dest, "bool");
            break;
        }
        case PICCOLO_TYPE_NIL: {
            sprintf(dest, "nil");
            break;
        }
        case PICCOLO_TYPE_ARRAY: {
            char buf[256];
            piccolo_getTypename(type->subtypes.listElem, buf);
            snprintf(dest, 256, "[%s]", buf);
            break;
        }
        case PICCOLO_TYPE_HASHMAP: {
            char keyBuf[256];
            piccolo_getTypename(type->subtypes.hashmap.key, keyBuf);
            char valBuf[256];
            piccolo_getTypename(type->subtypes.hashmap.val, valBuf);
            char strKeyBuf[256];
            strKeyBuf[0] = '\0';
            char* ptr = strKeyBuf;
            size_t sizeLeft = 256;
            for(int i = 0; i < type->subtypes.hashmap.strKeys.count; i++) {
                char strValBuf[256];
                piccolo_getTypename(type->subtypes.hashmap.strTypes.values[i], strValBuf);
                struct piccolo_Token* strKey = &type->subtypes.hashmap.strKeys.values[i];
                size_t taken = snprintf(ptr, sizeLeft, "'%.*s' : %s, ", strKey->length - 2, strKey->start + 1, strValBuf);
                ptr += taken;
                sizeLeft -= taken;
            }
            snprintf(dest, 256, "{%s%s : %s}", strKeyBuf, keyBuf, valBuf);
            break;
        }
        case PICCOLO_TYPE_FN: {
            dest += sprintf(dest, "fn ");
            size_t sizeLeft = 256 - 3;
            size_t taken;
            for(int i = 0; i < type->subtypes.fn.params.count; i++) {
                char buf[256];
                piccolo_getTypename(type->subtypes.fn.params.values[i], buf);
                taken = snprintf(dest, sizeLeft, i == 0 ? "%s" : ", %s", buf);
                dest += taken;
                sizeLeft -= taken;
            }
            taken = snprintf(dest, sizeLeft, type->subtypes.fn.params.count == 0 ? "-> " : " -> ");
            dest += taken;
            sizeLeft -= taken;
            char buf[256];
            piccolo_getTypename(type->subtypes.fn.result, buf);
            dest += snprintf(dest, sizeLeft, "%s", buf);
            break;
        }
        case PICCOLO_TYPE_PKG: {
            sprintf(dest, "pkg");
            break;
        }
        case PICCOLO_TYPE_UNION: {
            size_t sizeLeft = 256;
            for(int i = 0; i < type->subtypes.unionTypes.types.count; i++) {
                char buf[256];
                piccolo_getTypename(type->subtypes.unionTypes.types.values[i], buf);
                size_t taken = snprintf(dest, sizeLeft, i == 0 ? "%s" : " | %s", buf);
                dest += taken;
                sizeLeft -= taken;
            }
            break;
        }
    }
    dest = originalDest;
    dest[252] = '.';
    dest[253] = '.';
    dest[254] = '.';
    dest[255] = '\0';
}

static uint64_t typeToNum(struct piccolo_Type* type) { // TODO: make this more intelligent lol
    union {
        struct piccolo_Type* type;
        uint64_t ptr;
    } x;
    x.type = type;
    return x.ptr;
}

static void sortTypeArray(struct piccolo_TypeArray* types, int start, int size) { // TODO: perhaps make this part of the dynarr macro madness?
    if(size == 1 || size == 0)
        return;
    sortTypeArray(types, start, size / 2);
    sortTypeArray(types, start + size / 2, size - size / 2);
    struct picolo_Type* temp[size];
    int a = 0, b = 0;
    for(int i = 0; i < size; i++) {
        if(b == size - size / 2 || typeToNum(types->values[start + a]) < typeToNum(types->values[start + size / 2 + b])) {
            temp[i] = types->values[start + a];
            a++;
        } else {
            temp[i] = types->values[start + size / 2 + b];
            b++;
        }
    }
    memcpy(&types->values[start], temp, size * sizeof(struct piccolo_Type*));
}

void piccolo_freeType(struct piccolo_Engine* engine, struct piccolo_Type* type) {
    if(type->type == PICCOLO_TYPE_FN) {
        piccolo_freeTypeArray(engine, &type->subtypes.fn.params);
    }
    if(type->type == PICCOLO_TYPE_HASHMAP) {
        piccolo_freeTokenArray(engine, &type->subtypes.hashmap.strKeys);
        piccolo_freeTypeArray(engine, &type->subtypes.hashmap.strTypes);
    }
    if(type->type == PICCOLO_TYPE_UNION) {
        piccolo_freeTypeArray(engine, &type->subtypes.unionTypes.types);
    }
}

static bool isAny(struct piccolo_Type* type) {
    return type->type == PICCOLO_TYPE_ANY;
}

static bool isNum(struct piccolo_Type* type) {
    return type->type == PICCOLO_TYPE_NUM || type->type == PICCOLO_TYPE_ANY;
}

static bool isStr(struct piccolo_Type* type) {
    return type->type == PICCOLO_TYPE_STR || type->type == PICCOLO_TYPE_ANY;
}

static bool isBool(struct piccolo_Type* type) {
    return type->type == PICCOLO_TYPE_BOOL || type->type == PICCOLO_TYPE_ANY;
}

static bool isFn(struct piccolo_Type* type) {
    return type->type == PICCOLO_TYPE_FN || type->type == PICCOLO_TYPE_ANY;
}

static bool isPkg(struct piccolo_Type* type) {
    return type->type == PICCOLO_TYPE_PKG || type->type == PICCOLO_TYPE_ANY;
}

static bool isArr(struct piccolo_Type* type) {
    return type->type == PICCOLO_TYPE_ARRAY || type->type == PICCOLO_TYPE_ANY;
}

static bool isHashmap(struct piccolo_Type* type) {
    return type->type == PICCOLO_TYPE_HASHMAP || type->type == PICCOLO_TYPE_ANY;
}

static struct piccolo_Type* allocType(struct piccolo_Engine* engine, enum piccolo_TypeType type) {
    struct piccolo_Type* newType = PICCOLO_REALLOCATE("type", engine, NULL, 0, sizeof(struct piccolo_Type));
    newType->type = type;
    newType->next = engine->types;
    engine->types = newType;
    return newType;
}

struct piccolo_Type* piccolo_simpleType(struct piccolo_Engine* engine, enum piccolo_TypeType type) {
    struct piccolo_Type* curr = engine->types;
    while(curr != NULL) {
        if(curr->type == type)
            return curr;
        curr = curr->next;
    }
    return allocType(engine, type);
}

struct piccolo_Type* piccolo_arrayType(struct piccolo_Engine* engine, struct piccolo_Type* elemType) {
    struct piccolo_Type* curr = engine->types;
    while(curr != NULL) {
        if(curr->type == PICCOLO_TYPE_ARRAY && curr->subtypes.listElem == elemType)
            return curr;
        curr = curr->next;
    }
    struct piccolo_Type* result = allocType(engine, PICCOLO_TYPE_ARRAY);
    result->subtypes.listElem = elemType;
    return result;
}

struct piccolo_Type* piccolo_hashmapType(struct piccolo_Engine* engine, struct piccolo_Type* keyType, struct piccolo_Type* valType, struct piccolo_TokenArray* strKeys, struct piccolo_TypeArray* strTypes) {
    struct piccolo_Type* curr = engine->types;
    while(curr != NULL) {
        if(curr->type == PICCOLO_TYPE_HASHMAP) {
            bool allStrKeysMatch = true;
            for(int i = 0; i < strKeys->count; i++) {
                bool found = false;
                for(int j = 0; j < curr->subtypes.hashmap.strKeys.count; j++) {
                    struct piccolo_Token* otherToken = &curr->subtypes.hashmap.strKeys.values[j];
                    if(strKeys->values[i].length == otherToken->length &&
                        memcmp(strKeys->values[i].start, otherToken->start, strKeys->values[i].length) == 0) {
                        if(strTypes->values[i] == curr->subtypes.hashmap.strTypes.values[j]) {
                            found = true;
                            break;
                        }
                    }
                }
                if(!found)
                    allStrKeysMatch = false;
            }
            if(curr->subtypes.hashmap.key == keyType &&
                curr->subtypes.hashmap.val == valType && allStrKeysMatch) {
                piccolo_freeTokenArray(engine, strKeys);
                piccolo_freeTypeArray(engine, strTypes);
                return curr;
            }
        }
        curr = curr->next;
    }
    struct piccolo_Type* result = allocType(engine, PICCOLO_TYPE_HASHMAP);
    result->subtypes.hashmap.key = keyType;
    result->subtypes.hashmap.val = valType;
    result->subtypes.hashmap.strKeys = *strKeys;
    result->subtypes.hashmap.strTypes = *strTypes;
    return result;
}

static bool typeArrEq(struct piccolo_TypeArray* a, struct piccolo_TypeArray* b) {
    if(a->count != b->count)
        return false;
    for(int i = 0; i < a->count; i++)
        if(a->values[i] != b->values[i])
            return false;
    return true;
}

struct piccolo_Type* piccolo_fnType(struct piccolo_Engine* engine, struct piccolo_TypeArray* params, struct piccolo_Type* res) {
    struct piccolo_Type* curr = engine->types;
    while(curr != NULL) {
        if(curr->type == PICCOLO_TYPE_FN && curr->subtypes.fn.result == res && typeArrEq(params, &curr->subtypes.fn.params)) {
            piccolo_freeTypeArray(engine, params);
            return curr;
        }
        curr = curr->next;
    }
    struct piccolo_Type* fnType = allocType(engine, PICCOLO_TYPE_FN);
    fnType->subtypes.fn.params = *params;
    fnType->subtypes.fn.result = res;
    return fnType;
}

struct piccolo_Type* piccolo_pkgType(struct piccolo_Engine* engine, struct piccolo_Package* pkg) {
    struct piccolo_Type* curr = engine->types;
    while(curr != NULL) {
        if(curr->type == PICCOLO_TYPE_PKG && curr->subtypes.pkg == pkg)
            return curr;
        curr = curr->next;
    }
    struct piccolo_Type* pkgType = allocType(engine, PICCOLO_TYPE_PKG);
    pkgType->subtypes.pkg = pkg;
    return pkgType;
}

struct piccolo_Type* piccolo_unionType(struct piccolo_Engine* engine, struct piccolo_TypeArray* types) {
    sortTypeArray(types, 0, types->count);
    struct piccolo_Type* curr = engine->types;
    while(curr != NULL) {
        if(curr->type == PICCOLO_TYPE_UNION && typeArrEq(&curr->subtypes.unionTypes.types, types)) { // TODO: dedupe permutations of types. Atm, num | str and str | num are considered different and memory is wasted
            piccolo_freeTypeArray(engine, types);
            return curr;
        }
        curr = curr->next;
    }
    struct piccolo_Type* unionType = allocType(engine, PICCOLO_TYPE_UNION);
    unionType->subtypes.unionTypes.types = *types;
    return unionType;
}

static bool isSubtype(struct piccolo_Type* super, struct piccolo_Type* sub) {
    if(isAny(super) || isAny(sub))
        return true;
    if(super == sub)
        return true;
    if(super->type == PICCOLO_TYPE_UNION) {
        if(sub->type == PICCOLO_TYPE_UNION) {
            for(int i = 0; i < sub->subtypes.unionTypes.types.count; i++) {
                bool found = false;
                for(int j = 0; j < super->subtypes.unionTypes.types.count; j++) {
                    if(isSubtype(super->subtypes.unionTypes.types.values[j], sub->subtypes.unionTypes.types.values[i])) {
                        found = true;
                        break;
                    }
                }
                if(!found)
                    return false;
            }
            return true;
        } else {
            for(int i = 0; i < super->subtypes.unionTypes.types.count; i++) {
                if(isSubtype(super->subtypes.unionTypes.types.values[i], sub))
                    return true;
            }
        }
    }
    if(super->type == PICCOLO_TYPE_ARRAY) {
        if(sub->type == PICCOLO_TYPE_ARRAY) {
            return isSubtype(super->subtypes.listElem, sub->subtypes.listElem);
        }
    }
    if(super->type == PICCOLO_TYPE_HASHMAP) {
        if(sub->type == PICCOLO_TYPE_HASHMAP) {
            if(!isSubtype(super->subtypes.hashmap.key, sub->subtypes.hashmap.key))
                return false;
            if(!isSubtype(super->subtypes.hashmap.val, sub->subtypes.hashmap.val))
                return false;
            for(int i = 0; i < sub->subtypes.hashmap.strKeys.count; i++) {
                bool found = false;
                for(int j = 0; j < super->subtypes.hashmap.strKeys.count; j++) {
                    struct piccolo_Token* a = &sub->subtypes.hashmap.strKeys.values[i];
                    struct piccolo_Token* b = &super->subtypes.hashmap.strKeys.values[j];
                    if(a->length == b->length && memcmp(a->start, b->start, a->length) == 0) {
                        if(isSubtype(super->subtypes.hashmap.strTypes.values[j], sub->subtypes.hashmap.strTypes.values[i])) {
                            found = true;
                            break;
                        }
                    }
                }
                if(!found)
                    return false;
            }
            return true;
        }
    }
    return false;
}

static struct piccolo_Type* mergeTypes(struct piccolo_Engine* engine, struct piccolo_Type* a, struct piccolo_Type* b) {
    if(a == NULL)
        return b;
    if(b == NULL)
        return a;
    if(a == b)
        return a;
    if(isSubtype(a, b))
        return a;
    if(isSubtype(b, a))
        return b;
    struct piccolo_TypeArray res;
    piccolo_initTypeArray(&res);
    if(a->type == PICCOLO_TYPE_UNION) {
        for(int i = 0; i < a->subtypes.unionTypes.types.count; i++)
            piccolo_writeTypeArray(engine, &res, a->subtypes.unionTypes.types.values[i]);
    } else {
        piccolo_writeTypeArray(engine, &res, a);
    }
    if(b->type == PICCOLO_TYPE_UNION) {
        for(int i = 0; i < b->subtypes.unionTypes.types.count; i++) {
            struct piccolo_Type* t = b->subtypes.unionTypes.types.values[i];
            bool found = false;
            for(int i = 0; i < res.count; i++) {
                if(isSubtype(res.values[i], t)) {
                    found = true;
                    break;
                }
            }
            if(!found)
                piccolo_writeTypeArray(engine, &res, t);
        }
    } else {
        bool found = false;
        for(int i = 0; i < res.count; i++) {
            if(isSubtype(res.values[i], b)) {
                found = true;
                break;
            }
        }
        if(!found)
            piccolo_writeTypeArray(engine, &res, b);
    }
    return piccolo_unionType(engine, &res); // TODO: add union type
}

static struct piccolo_Type* getSubscriptType(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Type* valType, struct piccolo_Token* subscript) {
    #define IS_SUBSCRIPT(str) (subscript->length == strlen(str) && memcmp(str, subscript->start, subscript->length) == 0)
    if(isAny(valType)) {
        return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
    } else if(isArr(valType) || isStr(valType)) {
        if(IS_SUBSCRIPT("length")) {
            return piccolo_simpleType(engine, PICCOLO_TYPE_NUM);
        }
    } else if(isHashmap(valType)) {
        for(int i = 0; i < valType->subtypes.hashmap.strKeys.count; i++) {
            struct piccolo_Token* key = &valType->subtypes.hashmap.strKeys.values[i];
            if(key->length - 2 == subscript->length && memcmp(key->start + 1, subscript->start, subscript->length) == 0) {
                return valType->subtypes.hashmap.strTypes.values[i];
            }
        }

        struct piccolo_Type* keyType = valType->subtypes.hashmap.key;
        if(isStr(keyType)) {
            return valType->subtypes.hashmap.val;
        }
    } else if(isPkg(valType)) {
        struct piccolo_Package* pkg = valType->subtypes.pkg;
        if(pkg == NULL)
            return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
        struct piccolo_ObjString* indexStr = piccolo_copyString(engine, subscript->start, subscript->length);
        int globalIdx = piccolo_getGlobalTable(engine, &pkg->globalIdxs, indexStr);
        if(globalIdx != -1) {
            struct piccolo_Type* varType = pkg->types.values[globalIdx & ~PICCOLO_GLOBAL_SLOT_MUTABLE_BIT];
            return varType == NULL ? piccolo_simpleType(engine, PICCOLO_TYPE_ANY) : varType;
        }
    }
    char buf[256];
    piccolo_getTypename(valType, buf);
    piccolo_compilationError(engine, compiler, subscript->charIdx, "Property '%.*s' does not exsist on %s.", subscript->length, subscript->start, buf);
    return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
    #undef IS_SUBSCRIPT
}

static struct piccolo_Type* getIndexType(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_Type* targetType, struct piccolo_Type* indexType, int charIdx) {
    if(isAny(targetType))
        return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
    if(isArr(targetType) && isNum(indexType)) {
        return targetType->subtypes.listElem;
    }
    if(isStr(targetType) && isNum(indexType))
        return piccolo_simpleType(engine, PICCOLO_TYPE_STR);
    if(isHashmap(targetType)) {
        struct piccolo_Type* keyType = targetType->subtypes.hashmap.key;
        if(isSubtype(keyType, indexType))
            return targetType->subtypes.hashmap.val;
    }
    if(isPkg(targetType)) {
        if(isStr(indexType))
            return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
    }

    char targetBuf[256];
    piccolo_getTypename(targetType, targetBuf);
    char indexBuf[256];
    piccolo_getTypename(indexType, indexBuf);
    piccolo_compilationError(engine, compiler, charIdx, "Cannot index %s with %s.", targetBuf, indexBuf);
    return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
}

static struct piccolo_Type* getType(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_ExprNode* expr) {
    switch(expr->type) {
        case PICCOLO_EXPR_LITERAL: {
            struct piccolo_LiteralNode* literal = (struct piccolo_LiteralNode*)expr;
            switch(literal->token.type) {
                case PICCOLO_TOKEN_NUM:
                    return piccolo_simpleType(engine, PICCOLO_TYPE_NUM);
                case PICCOLO_TOKEN_STRING:
                    return piccolo_simpleType(engine, PICCOLO_TYPE_STR);
                case PICCOLO_TOKEN_FALSE:
                case PICCOLO_TOKEN_TRUE:
                    return piccolo_simpleType(engine, PICCOLO_TYPE_BOOL);
                case PICCOLO_TOKEN_NIL:
                    return piccolo_simpleType(engine, PICCOLO_TYPE_NIL);
                default:
                    return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            }
        }
        case PICCOLO_EXPR_ARRAY_LITERAL: {
            struct piccolo_ArrayLiteralNode* arrayLiteral = (struct piccolo_ArrayLiteralNode*)expr;
            struct piccolo_Type* elemType = NULL;
            struct piccolo_ExprNode* curr = arrayLiteral->first;
            while(curr != NULL) {
                elemType = mergeTypes(engine, elemType, piccolo_getType(engine, compiler, curr));
                curr = curr->nextExpr;
            }
            if(elemType == NULL)
                elemType = piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            return piccolo_arrayType(engine, elemType);
        }
        case PICCOLO_EXPR_HASHMAP_ENTRY: {
            return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
        }
        case PICCOLO_EXPR_HASHMAP_LITERAL: {
            struct piccolo_HashmapLiteralNode* hashmapLiteral = (struct piccolo_HashmapLiteralNode*)expr;
            struct piccolo_HashmapEntryNode* currEntry = hashmapLiteral->first;

            struct piccolo_Type* keyType = NULL;
            struct piccolo_Type* valType = NULL;

            struct piccolo_TokenArray strKeys;
            struct piccolo_TypeArray strTypes;
            piccolo_initTokenArray(&strKeys);
            piccolo_initTypeArray(&strTypes);
            
            while(currEntry != NULL) {
                struct piccolo_ExprNode* keyExpr = currEntry->key;
                struct piccolo_Type* currValType = piccolo_getType(engine, compiler, currEntry->value);
                
                if(keyExpr->type == PICCOLO_EXPR_LITERAL) {
                    struct piccolo_LiteralNode* keyLiteral = keyExpr;
                    if(keyLiteral->token.type == PICCOLO_TOKEN_STRING) {
                        piccolo_writeTokenArray(engine, &strKeys, keyLiteral->token);
                        piccolo_writeTypeArray(engine, &strTypes, currValType);
                    }
                }
                keyType = mergeTypes(engine, keyType, piccolo_getType(engine, compiler, keyExpr));
                valType = mergeTypes(engine, valType, currValType);
                currEntry = currEntry->expr.nextExpr;
            }
            if(keyType == NULL)
                keyType = piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            if(valType == NULL)
                valType = piccolo_simpleType(engine, PICCOLO_TYPE_ANY);

            struct piccolo_Type* result = piccolo_hashmapType(engine, keyType, valType, &strKeys, &strTypes);
            return result;
        }
        case PICCOLO_EXPR_VAR: {
            struct piccolo_VarNode* var = (struct piccolo_VarNode*)expr;
            if(var->decl == NULL)
                return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            return var->decl->typed ? piccolo_getType(engine, compiler, var->decl->value) : piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
        }
        case PICCOLO_EXPR_RANGE: {
            struct piccolo_RangeNode* range = (struct piccolo_RangeNode*)expr;
            struct piccolo_Type* lType = piccolo_getType(engine, compiler, range->left);
            struct piccolo_Type* rType = piccolo_getType(engine, compiler, range->right);
            if(!(isNum(lType) && isNum(rType))) {
                char lBuf[256];
                piccolo_getTypename(lType, lBuf);
                char rBuf[256];
                piccolo_getTypename(rType, rBuf);
                piccolo_compilationError(engine, compiler, range->charIdx, "Cannot create range between %s and %s.", lBuf, rBuf);
                return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            }
            return piccolo_arrayType(engine, piccolo_simpleType(engine, PICCOLO_TYPE_NUM));
        }
        case PICCOLO_EXPR_SUBSCRIPT: {
            struct piccolo_SubscriptNode* subscript = (struct piccolo_SubscriptNode*)expr;
            return getSubscriptType(engine, compiler, piccolo_getType(engine, compiler, subscript->value), &subscript->subscript);
        }
        case PICCOLO_EXPR_INDEX: {
            struct piccolo_IndexNode* index = (struct piccolo_IndexNode*)expr;
            struct piccolo_Type* targetType = piccolo_getType(engine, compiler, index->target);
            struct piccolo_Type* indexType = piccolo_getType(engine, compiler, index->index);
            return getIndexType(engine, compiler, targetType, indexType, index->charIdx);
        }
        case PICCOLO_EXPR_UNARY: {
            struct piccolo_UnaryNode* unary = (struct piccolo_UnaryNode*)expr;
            struct piccolo_Type* valType = piccolo_getType(engine, compiler, unary->value);
            switch(unary->op.type) {
                case PICCOLO_TOKEN_MINUS: {
                    if(isNum(valType)) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_NUM);
                    } else {
                        char buf[256];
                        piccolo_getTypename(valType, buf);
                        piccolo_compilationError(engine, compiler, unary->op.charIdx, "Cannot negate %s.", buf);
                        return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
                    }
                }
                case PICCOLO_TOKEN_BANG: {
                    if(isBool(valType)) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_BOOL);
                    } else {
                        char buf[256];
                        piccolo_getTypename(valType, buf);
                        piccolo_compilationError(engine, compiler, unary->op.charIdx, "Cannot invert %s.", buf);
                        return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
                    }
                }
            }
        }
        case PICCOLO_EXPR_BINARY: {
            struct piccolo_BinaryNode* binary = (struct piccolo_BinaryNode*)expr;
            struct piccolo_Type* aType = piccolo_getType(engine, compiler, binary->a);
            struct piccolo_Type* bType = piccolo_getType(engine, compiler, binary->b);
            if(isAny(aType) && isAny(bType))
                return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            char aBuf[256];
            piccolo_getTypename(aType, aBuf);
            char bBuf[256];
            piccolo_getTypename(bType, bBuf);
            switch(binary->op.type) {
                case PICCOLO_TOKEN_PLUS: {
                    if(isNum(aType) && isNum(bType)) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_NUM);
                    }
                    if(isStr(aType) && isStr(bType)) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_STR);
                    }
                    if(isArr(aType) && isArr(bType)) {
                        return piccolo_arrayType(engine, mergeTypes(engine, aType->subtypes.listElem, bType->subtypes.listElem));
                    }
                    piccolo_compilationError(engine, compiler, binary->op.charIdx, "Cannot add %s and %s.", aBuf, bBuf);
                    break;
                }
                case PICCOLO_TOKEN_MINUS: {
                    if(isNum(aType) && isNum(bType)) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_NUM);
                    }
                    piccolo_compilationError(engine, compiler, binary->op.charIdx, "Cannot subtract %s from %s.", bBuf, aBuf);
                    break;
                }
                case PICCOLO_TOKEN_SLASH: {
                    if(isNum(aType) && isNum(bType)) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_NUM);
                    }
                    piccolo_compilationError(engine, compiler, binary->op.charIdx, "Cannot divide %s by %s.", aBuf, bBuf);
                    break;
                }
                case PICCOLO_TOKEN_PERCENT: {
                    if(isNum(aType) && isNum(bType)) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_NUM);
                    }
                    piccolo_compilationError(engine, compiler, binary->op.charIdx, "Cannot modulo %s by %s.", aBuf, bBuf);
                    break;
                }
                case PICCOLO_TOKEN_STAR: {
                    if(isNum(aType) && (isNum(bType) || isStr(bType) || isArr(bType))) {
                        return bType;
                    }
                    if(isNum(bType) && (isNum(aType) || isStr(aType) || isArr(aType))) {
                        return aType;
                    }
                    piccolo_compilationError(engine, compiler, binary->op.charIdx, "Cannot multiply %s by %s.", aBuf, bBuf);
                    break;
                }
                case PICCOLO_TOKEN_AND: {
                    if(isBool(aType) && isBool(bType)) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_BOOL);
                    }
                    piccolo_compilationError(engine, compiler, binary->op.charIdx, "Cannot and %s and %s.", aBuf, bBuf);
                    break;
                }
                case PICCOLO_TOKEN_OR: {
                    if(isBool(aType) && isBool(bType)) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_BOOL);
                    }
                    piccolo_compilationError(engine, compiler, binary->op.charIdx, "Cannot or %s and %s.", aBuf, bBuf);
                    break;
                }
                case PICCOLO_TOKEN_LESS:
                case PICCOLO_TOKEN_GREATER:
                case PICCOLO_TOKEN_LESS_EQ:
                case PICCOLO_TOKEN_GREATER_EQ: {
                    if(isNum(aType) && isNum(bType))
                        return piccolo_simpleType(engine, PICCOLO_TYPE_BOOL);
                    piccolo_compilationError(engine, compiler, binary->op.charIdx, "Cannot compare %s and %s.", aBuf, bBuf);
                    break;
                }
                case PICCOLO_TOKEN_EQ_EQ:
                case PICCOLO_TOKEN_BANG_EQ:
                    return piccolo_simpleType(engine, PICCOLO_TYPE_BOOL);
                case PICCOLO_TOKEN_IN: {
                    if(isAny(bType) || (bType->type == PICCOLO_TYPE_HASHMAP && isSubtype(bType->subtypes.hashmap.key, aType))) {
                        return piccolo_simpleType(engine, PICCOLO_TYPE_BOOL);
                    }
                    piccolo_compilationError(engine, compiler, binary->op.charIdx, "Cannot check if %s is in %s.", aBuf, bBuf);
                }
            }
            return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
        }
        case PICCOLO_EXPR_BLOCK: {
            struct piccolo_BlockNode* block = (struct piccolo_BlockNode*)expr;
            struct piccolo_Type* res = piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            struct piccolo_ExprNode* curr = block->first;
            while(curr != NULL) {
                res = piccolo_getType(engine, compiler, curr);
                curr = curr->nextExpr;
            }
            return res;
        }
        case PICCOLO_EXPR_FN_LITERAL: {
            struct piccolo_FnLiteralNode* fn = (struct piccolo_FnLiteralNode*)expr;
            struct piccolo_TypeArray paramTypes;
            piccolo_initTypeArray(&paramTypes);
            for(int i = 0; i < fn->params.count; i++)
                piccolo_writeTypeArray(engine, &paramTypes, piccolo_simpleType(engine, PICCOLO_TYPE_ANY));
            struct piccolo_Type* resType = piccolo_getType(engine, compiler, fn->value);
            return piccolo_fnType(engine, &paramTypes, resType);
        }
        case PICCOLO_EXPR_VAR_DECL: {
            struct piccolo_VarDeclNode* varDecl = (struct piccolo_VarDeclNode*)expr;
            return piccolo_getType(engine, compiler, varDecl->value);
        }
        case PICCOLO_EXPR_VAR_SET: {
            struct piccolo_VarSetNode* varSet = (struct piccolo_VarSetNode*)expr;
            struct piccolo_Type* valType = piccolo_getType(engine, compiler, varSet->value);
            if(varSet->decl == NULL || !varSet->decl->typed)    
                return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            struct piccolo_Type* targetType = piccolo_getType(engine, compiler, varSet->decl->value);
            if(!isSubtype(targetType, valType)) {
                char varTypeBuf[256];
                piccolo_getTypename(targetType, varTypeBuf);
                char valTypeBuf[256];
                piccolo_getTypename(valType, valTypeBuf);
                piccolo_compilationError(engine, compiler, varSet->name.charIdx, "Cannot assign %s to variable of type %s.", valTypeBuf, varTypeBuf);
            }
            return valType;
        }
        case PICCOLO_EXPR_SUBSCRIPT_SET: {
            struct piccolo_SubscriptSetNode* subscriptSet = (struct piccolo_SubscriptSetNode*)expr;
            struct piccolo_Type* newValType = piccolo_getType(engine, compiler, subscriptSet->value);
            struct piccolo_Type* targetType = getSubscriptType(engine, compiler, piccolo_getType(engine, compiler, subscriptSet->target), &subscriptSet->subscript);
            if(isSubtype(targetType, newValType)) {
                return newValType;
            }
            char newValBuf[256];
            piccolo_getTypename(newValType, newValBuf);
            char targetBuf[256];
            piccolo_getTypename(targetType, targetBuf);
            piccolo_compilationError(engine, compiler, subscriptSet->subscript.charIdx, "Cannot assign %s to %s.", newValBuf, targetBuf);
            return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
        }
        case PICCOLO_EXPR_INDEX_SET: {
            struct piccolo_IndexSetNode* indexSet = (struct piccolo_IndexSetNode*)expr;
            struct piccolo_Type* targetType = piccolo_getType(engine, compiler, indexSet->target);
            struct piccolo_Type* indexType = piccolo_getType(engine, compiler, indexSet->index);
            struct piccolo_Type* valType = getIndexType(engine, compiler, targetType, indexType, indexSet->charIdx);
            struct piccolo_Type* newValType = piccolo_getType(engine, compiler, indexSet->value);
            if(isSubtype(valType, newValType)) {
                return newValType;
            }
            char newValBuf[256];
            piccolo_getTypename(newValType, newValBuf);
            char valBuf[256];
            piccolo_getTypename(valType, valBuf);
            piccolo_compilationError(engine, compiler, indexSet->charIdx, "Cannot assign %s to %s.", newValBuf, valBuf);
            return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
        }
        case PICCOLO_EXPR_IF: {
            struct piccolo_IfNode* ifNode = (struct piccolo_IfNode*)expr;
            struct piccolo_Type* conditionType = piccolo_getType(engine, compiler, ifNode->condition);
            struct piccolo_Type* aType = piccolo_getType(engine, compiler, ifNode->trueVal);
            struct piccolo_Type* bType = ifNode->falseVal == NULL ? NULL : piccolo_getType(engine, compiler, ifNode->falseVal);
            if(!isBool(conditionType)) {
                piccolo_compilationError(engine, compiler, ifNode->conditionCharIdx, "Condition must be a bool.");
            }
            return mergeTypes(engine, aType, bType);
        }
        case PICCOLO_EXPR_WHILE: {
            struct piccolo_WhileNode* whileNode = (struct piccolo_WhileNode*)expr;
            struct piccolo_Type* conditionType = piccolo_getType(engine, compiler, whileNode->condition);
            struct piccolo_Type* valType = piccolo_getType(engine, compiler, whileNode->value);
            if(!isBool(conditionType)) {
                piccolo_compilationError(engine, compiler, whileNode->conditionCharIdx, "Condition must be a bool.");
            }
            return piccolo_arrayType(engine, valType);
        }
        case PICCOLO_EXPR_FOR: {
            struct piccolo_ForNode* forNode = (struct piccolo_ForNode*)expr;
            struct piccolo_Type* containerType = piccolo_getType(engine, compiler, forNode->container);
            struct piccolo_Type* valType = piccolo_getType(engine, compiler, forNode->value);
            if(!isArr(containerType) && !isHashmap(containerType) && !isStr(containerType)) {
                char buf[256];
                piccolo_getTypename(containerType, buf);
                piccolo_compilationError(engine, compiler, forNode->containerCharIdx, "Cannot iterate over %s.", buf);
            }
            return piccolo_arrayType(engine, valType);
        }

        case PICCOLO_EXPR_CALL: {
            struct piccolo_CallNode* call = (struct piccolo_CallNode*)expr;
            struct piccolo_Type* fnType = piccolo_getType(engine, compiler, call->function);
            if(isAny(fnType)) {
                struct piccolo_ExprNode* arg = call->firstArg;
                while(arg != NULL) {
                    piccolo_getType(engine, compiler, arg);
                    arg = arg->nextExpr;
                }
                return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            }
            if(!isFn(fnType))
                goto callError;
            struct piccolo_ExprNode* arg = call->firstArg;
            int currArg = 0;
            while(arg != NULL) {
                struct piccolo_Type* argType = piccolo_getType(engine, compiler, arg);
                if(!isAny(fnType) && (currArg >= fnType->subtypes.fn.params.count || !isSubtype(fnType->subtypes.fn.params.values[currArg], argType))) {
                    goto callError;
                }
                arg = arg->nextExpr;
                currArg++;
            }
            if(currArg < fnType->subtypes.fn.params.count)
                goto callError;
            if(isAny(fnType))
                return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
            return fnType->subtypes.fn.result;

            callError:
            currArg = 0;
            char fnBuf[256];
            piccolo_getTypename(fnType, fnBuf);
            char paramBuf[256];
            paramBuf[0] = 0;
            char* paramDest = paramBuf;
            arg = call->firstArg;
            while(arg != NULL) {
                char typeBuf[256];
                piccolo_getTypename(piccolo_getType(engine, compiler, arg), typeBuf);
                paramDest += sprintf(paramDest, arg->nextExpr == NULL ? "%s" : "%s, ", typeBuf);
                arg = arg->nextExpr;
            }
            piccolo_compilationError(engine, compiler, call->charIdx, "Cannot call %s with params ( %s )", fnBuf, paramBuf);
            return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
        }
        case PICCOLO_EXPR_IMPORT: {
            struct piccolo_ImportNode* import = (struct piccolo_ImportNode*)expr;
            struct piccolo_Package* package = piccolo_resolveImport(engine, compiler, compiler->package->packageName, import);
            return piccolo_pkgType(engine, package);
        }
    }
    return piccolo_simpleType(engine, PICCOLO_TYPE_ANY);
}

struct piccolo_Type* piccolo_getType(struct piccolo_Engine* engine, struct piccolo_Compiler* compiler, struct piccolo_ExprNode* expr) {
    if(expr->resultType != NULL)
        return expr->resultType;
    struct piccolo_Type* type = getType(engine, compiler, expr);
    expr->resultType = type;
    return type;
}

#include "picStdlib.h"
#include "../embedding.h"
#include "../util/memory.h"
#include "../util/strutil.h"

#include <stdio.h>
#include <string.h>

static piccolo_Value getCodeNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value val = argv[0];
    if(!PICCOLO_IS_STRING(val)) {
        piccolo_runtimeError(engine, "Argument must be a string.");
        return PICCOLO_NIL_VAL();
    }
    struct piccolo_ObjString* str = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(val);
    if(str->utf8Len != 1) {
        piccolo_runtimeError(engine, "Argument must be a single character string.");
        return PICCOLO_NIL_VAL();
    }
    int charSize = piccolo_strutil_utf8Chars(*str->string);
    int code = 0;
    if(charSize == 1) {
        code = *str->string;
    }
    if(charSize == 2) {
        code = ((*str->string & 0x1F) << 6) + (*(str->string + 1) & 0x3F);
    }
    if(charSize == 3) {
        code = ((*str->string & 0x0F) << 12) + ((*(str->string + 1) & 0x3F) << 6) + (*(str->string + 2) & 0x3F);
    }
    if(charSize == 4) {
        code = ((*str->string & 0x07) << 18) + ((*(str->string + 1) & 0x3F) << 12) + ((*(str->string + 2) & 0x3F) << 6) + (*(str->string + 3) & 0x3F);
    }
    return PICCOLO_NUM_VAL(code);
}

static piccolo_Value numToStrNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value val = argv[0];
    if(!PICCOLO_IS_NUM(val)) {
        piccolo_runtimeError(engine, "Argument must be a number.");
        return PICCOLO_NIL_VAL();
    }
    char buf[32];
    sprintf(buf, "%g", PICCOLO_AS_NUM(val));
    return PICCOLO_OBJ_VAL(piccolo_copyString(engine, buf, strlen(buf)));
}

void piccolo_addStrLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* str = piccolo_createPackage(engine);
    str->packageName = "str";
    struct piccolo_Type* strType = piccolo_simpleType(engine, PICCOLO_TYPE_STR);
    struct piccolo_Type* num = piccolo_simpleType(engine, PICCOLO_TYPE_NUM);
    piccolo_defineGlobalWithType(engine, str, "utfCode", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, getCodeNative)), piccolo_makeFnType(engine, num, 1, strType));
    piccolo_defineGlobalWithType(engine, str, "numToStr", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, numToStrNative)), piccolo_makeFnType(engine, strType, 1, num));
}

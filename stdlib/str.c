
#include "picStdlib.h"
#include "../embedding.h"
#include "../util/memory.h"
#include "../util/strutil.h"

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

void piccolo_addStrLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* str = piccolo_createPackage(engine);
    str->packageName = "str";
    piccolo_defineGlobal(engine, str, "utfCode", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, getCodeNative)));
}
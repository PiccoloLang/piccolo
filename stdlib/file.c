
#include "picStdlib.h"
#include "../util/file.h"
#include <stdio.h>
#include <string.h>
#include "../embedding.h"
#include "../gc.h"
#include "../util/strutil.h"

static piccolo_Value readNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value pathVal = argv[0];
    if(!PICCOLO_IS_OBJ(pathVal) || PICCOLO_AS_OBJ(pathVal)->type != PICCOLO_OBJ_STRING) {
        piccolo_runtimeError(engine, "Path must be a string.");
        return PICCOLO_NIL_VAL();
    }
    char* path = ((struct piccolo_ObjString*)PICCOLO_AS_OBJ(pathVal))->string;
    char* contents = piccolo_readFile(path);
    if(contents == NULL) {
        piccolo_runtimeError(engine, "Could not read file.");
        return PICCOLO_NIL_VAL();
    }
    struct piccolo_ObjString* stringObj = piccolo_takeString(engine, contents);
    return PICCOLO_OBJ_VAL(stringObj);
}

static piccolo_Value writeNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 2) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value pathVal = argv[0];
    if(!PICCOLO_IS_OBJ(pathVal) || PICCOLO_AS_OBJ(pathVal)->type != PICCOLO_OBJ_STRING) {
        piccolo_runtimeError(engine, "Path must be a string.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value data = argv[1];
    if(!PICCOLO_IS_OBJ(data) || PICCOLO_AS_OBJ(data)->type != PICCOLO_OBJ_STRING) {
        piccolo_runtimeError(engine, "Data must be a string.");
        return PICCOLO_NIL_VAL();
    }
    FILE* file = fopen(((struct piccolo_ObjString*)PICCOLO_AS_OBJ(pathVal))->string, "w");
    if(file == NULL) {
        piccolo_runtimeError(engine, "Could not open file.");
        return PICCOLO_NIL_VAL();
    }
    fprintf(file, "%s", ((struct piccolo_ObjString*)PICCOLO_AS_OBJ(data))->string);
    fclose(file);
    return PICCOLO_NIL_VAL();
}

struct file {
    FILE* file;
    piccolo_Value path, mode;
    piccolo_Value write, readChar, close;
};

static void gcMarkFile(void* payload) {
    struct file* file = (struct file*)payload;
    piccolo_gcMarkValue(file->path);
    piccolo_gcMarkValue(file->mode);

    piccolo_gcMarkValue(file->write);
    piccolo_gcMarkValue(file->readChar);
    piccolo_gcMarkValue(file->close);
}

static piccolo_Value indexFile(void* payload, struct piccolo_Engine* engine, piccolo_Value key, bool set, piccolo_Value value) {
    struct file* file = (struct file*)payload;
    if(!PICCOLO_IS_OBJ(key) || PICCOLO_AS_OBJ(key)->type != PICCOLO_OBJ_STRING) {
        piccolo_runtimeError(engine, "Property must be a string.");
        return PICCOLO_NIL_VAL();
    }
    struct piccolo_ObjString* keyStr = PICCOLO_AS_OBJ(key);
    if(strcmp(keyStr->string, "path") == 0) {
        if(set) {
            piccolo_runtimeError(engine, "Cannot set path.");
            return PICCOLO_NIL_VAL();
        }
        return file->path;
    }
    if(strcmp(keyStr->string, "mode") == 0) {
        if(set) {
            piccolo_runtimeError(engine, "Cannot set mode.");
            return PICCOLO_NIL_VAL();
        }
        return file->mode;
    }

    if(strcmp(keyStr->string, "write") == 0) {
        if(set) {
            piccolo_runtimeError(engine, "Cannot set write.");
            return PICCOLO_NIL_VAL();
        }
        return file->write;
    }
    if(strcmp(keyStr->string, "readChar") == 0) {
        if(set) {
            piccolo_runtimeError(engine, "Cannot set readChar.");
            return PICCOLO_NIL_VAL();
        }
        return file->readChar;
    }
    if(strcmp(keyStr->string, "close") == 0) {
        if(set) {
            piccolo_runtimeError(engine, "Cannot set close.");
            return PICCOLO_NIL_VAL();
        }
        return file->close;
    }

    piccolo_runtimeError(engine, "Invalid property.");
    return PICCOLO_NIL_VAL();
}

static piccolo_Value fileWriteNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 1) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value data = argv[0];
    if(!PICCOLO_IS_OBJ(data) || PICCOLO_AS_OBJ(data)->type != PICCOLO_OBJ_STRING) {
        piccolo_runtimeError(engine, "Data must be a string.");
        return PICCOLO_NIL_VAL();
    }
    struct file* file = PICCOLO_GET_PAYLOAD(PICCOLO_AS_OBJ(self), struct file);
    struct piccolo_ObjString* str = PICCOLO_AS_OBJ(data);
    fprintf(file->file, "%s", str->string);
    return PICCOLO_NIL_VAL();
}

static piccolo_Value fileReadCharNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    struct file* file = PICCOLO_GET_PAYLOAD(PICCOLO_AS_OBJ(self), struct file);
    char c = fgetc(file->file);
    if(c == EOF) {
        return PICCOLO_NIL_VAL();
    }
    char str[5];
    str[0] = c;
    int chars = piccolo_strutil_utf8Chars(c);
    for(int i = 1; i < chars; i++) {
        str[i] = fgetc(file->file);
    }
    str[chars] = '\0';
    return PICCOLO_OBJ_VAL(piccolo_copyString(engine, str, chars));
}

static piccolo_Value fileCloseNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 0) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    struct file* file = PICCOLO_GET_PAYLOAD(PICCOLO_AS_OBJ(self), struct file);
    fclose(file->file);
    return PICCOLO_NIL_VAL();
}

static piccolo_Value openNative(struct piccolo_Engine* engine, int argc, piccolo_Value* argv, piccolo_Value self) {
    if(argc != 2) {
        piccolo_runtimeError(engine, "Wrong argument count.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value pathVal = argv[0];
    if(!PICCOLO_IS_OBJ(pathVal) || PICCOLO_AS_OBJ(pathVal)->type != PICCOLO_OBJ_STRING) {
        piccolo_runtimeError(engine, "Path must be a string.");
        return PICCOLO_NIL_VAL();
    }
    piccolo_Value modeVal = argv[1];
    if(!PICCOLO_IS_OBJ(modeVal) || PICCOLO_AS_OBJ(modeVal)->type != PICCOLO_OBJ_STRING) {
        piccolo_runtimeError(engine, "Invalid file mode.");
        return PICCOLO_NIL_VAL();
    }
    struct piccolo_ObjString* mode = (struct piccolo_ObjString*)PICCOLO_AS_OBJ(modeVal);
    if(mode->len != 1) {
        piccolo_runtimeError(engine, "Invalid file mode.");
        return PICCOLO_NIL_VAL();
    }
    char modeChar = mode->string[0];
    if(modeChar != 'r' && modeChar != 'w') {
        piccolo_runtimeError(engine, "Invalid file mode.");
        return PICCOLO_NIL_VAL();
    }
    char* path = ((struct piccolo_ObjString*)PICCOLO_AS_OBJ(pathVal))->string;
    FILE* file = fopen(path, modeChar == 'r' ? "r" : "w");
    if(file == NULL) {
        piccolo_runtimeError(engine, "Could not open file.");
        return PICCOLO_NIL_VAL();
    }
    struct piccolo_ObjNativeStruct* fileObj = PICCOLO_ALLOCATE_NATIVE_STRUCT(engine, struct file, "file");
    fileObj->gcMark = gcMarkFile;
    fileObj->index = indexFile;
    struct file* payload = PICCOLO_GET_PAYLOAD(fileObj, struct file);
    payload->file = file;
    payload->path = pathVal;
    payload->mode = modeVal;

    payload->write = PICCOLO_OBJ_VAL(piccolo_makeBoundNative(engine, fileWriteNative, PICCOLO_OBJ_VAL(fileObj)));
    payload->readChar = PICCOLO_OBJ_VAL(piccolo_makeBoundNative(engine, fileReadCharNative, PICCOLO_OBJ_VAL(fileObj)));
    payload->close = PICCOLO_OBJ_VAL(piccolo_makeBoundNative(engine, fileCloseNative, PICCOLO_OBJ_VAL(fileObj)));

    return PICCOLO_OBJ_VAL(fileObj);
}

void piccolo_addFileLib(struct piccolo_Engine* engine) {
    struct piccolo_Package* file = piccolo_createPackage(engine);
    file->packageName = "file";
    piccolo_defineGlobal(engine, file, "read", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, readNative)));
    piccolo_defineGlobal(engine, file, "write", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, writeNative)));
    piccolo_defineGlobal(engine, file, "open", PICCOLO_OBJ_VAL(piccolo_makeNative(engine, openNative)));
}
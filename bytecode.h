
#ifndef PICCOLO_BYTECODE_H
#define PICCOLO_BYTECODE_H

#include <stddef.h>
#include <stdint.h>

#include "util/dynarray.h"
#include "value.h"

typedef enum {
    OP_RETURN,
    OP_CONST,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_EQUAL,
    OP_POP_STACK,

    OP_GET_GLOBAL,
    OP_GET_STACK,
    OP_SET,

    OP_JUMP,
    OP_JUMP_FALSE,

    OP_CALL
} piccolo_OpCode;

PICCOLO_DYNARRAY_HEADER(uint8_t, Byte)
PICCOLO_DYNARRAY_HEADER(int, Int)

struct piccolo_Bytecode {
    struct piccolo_ByteArray code;
    struct piccolo_IntArray charIdxs;
    struct piccolo_ValueArray constants;
} piccolo_Bytecode;

void piccolo_initBytecode(struct piccolo_Bytecode* bytecode);
void piccolo_freeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode);

void piccolo_writeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, uint8_t byte, int charIdx);
void piccolo_writeParameteredBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, uint8_t byte, uint16_t param, int charIdx);
void piccolo_writeConst(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, piccolo_Value value, int charIdx);
void piccolo_patchParam(struct piccolo_Bytecode* bytecode, int addr, uint16_t param);

#endif

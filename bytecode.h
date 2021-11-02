
#ifndef PICCOLO_BYTECODE_H
#define PICCOLO_BYTECODE_H

#include <stddef.h>
#include <stdint.h>

#include "util/dynarray.h"
#include "value.h"

typedef enum {
    PICCOLO_OP_RETURN,
    PICCOLO_OP_CONST,
    PICCOLO_OP_ADD, PICCOLO_OP_SUB, PICCOLO_OP_MUL, PICCOLO_OP_DIV, PICCOLO_OP_MOD,
    PICCOLO_OP_EQUAL, PICCOLO_OP_GREATER, PICCOLO_OP_LESS,
    PICCOLO_OP_NOT,

    PICCOLO_OP_POP_STACK,
    PICCOLO_OP_PEEK_STACK,
    PICCOLO_OP_SWAP_STACK,

    PICCOLO_OP_CREATE_ARRAY,
    PICCOLO_OP_CREATE_RANGE,
    PICCOLO_OP_GET_IDX,
    PICCOLO_OP_SET_IDX,

    PICCOLO_OP_GET_GLOBAL,
    PICCOLO_OP_SET_GLOBAL,
    PICCOLO_OP_GET_LOCAL,
    PICCOLO_OP_SET_LOCAL,

    PICCOLO_OP_JUMP,
    PICCOLO_OP_JUMP_FALSE,
    PICCOLO_OP_REV_JUMP,
    PICCOLO_OP_REV_JUMP_FALSE,

    PICCOLO_OP_CALL,

    PICCOLO_OP_CLOSURE,
    PICCOLO_OP_GET_UPVAL,
    PICCOLO_OP_SET_UPVAL,
    PICCOLO_OP_CLOSE_UPVALS,

    PICCOLO_OP_GET_LEN,
    PICCOLO_OP_APPEND,

    PICCOLO_OP_EXECUTE_PACKAGE
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

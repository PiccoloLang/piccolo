
#include "bytecode.h"

PICCOLO_DYNARRAY_IMPL(uint8_t, Byte)
PICCOLO_DYNARRAY_IMPL(int, Int)

void piccolo_initBytecode(struct piccolo_Bytecode* bytecode) {
    piccolo_initByteArray(&bytecode->code);
    piccolo_initIntArray(&bytecode->lines);
    piccolo_initValueArray(&bytecode->constants);
}

void piccolo_freeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode) {
    piccolo_freeByteArray(engine, &bytecode->code);
    piccolo_freeIntArray(engine, &bytecode->lines);
    piccolo_freeValueArray(engine, &bytecode->constants);
}

void piccolo_writeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, uint8_t byte, int line) {
    piccolo_writeByteArray(engine, &bytecode->code, byte);
    piccolo_writeIntArray(engine, &bytecode->lines, line);
}

void piccolo_writeConst(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, piccolo_Value constant, int line) {
    int constIdx = bytecode->constants.count;

    piccolo_writeValueArray(engine, &bytecode->constants, constant);
    piccolo_writeBytecode(engine, bytecode, OP_CONST, line);
    piccolo_writeBytecode(engine, bytecode, (constIdx & 0xFF00) >> 8, line);
    piccolo_writeBytecode(engine, bytecode, (constIdx & 0x00FF) >> 0, line);
}
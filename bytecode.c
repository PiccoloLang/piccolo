
#include "bytecode.h"

PICCOLO_DYNARRAY_IMPL(uint8_t, Byte)
PICCOLO_DYNARRAY_IMPL(int, Int)

void piccolo_initBytecode(struct piccolo_Bytecode* bytecode) {
    piccolo_initByteArray(&bytecode->code);
    piccolo_initIntArray(&bytecode->charIdxs);
    piccolo_initValueArray(&bytecode->constants);
}

void piccolo_freeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode) {
    piccolo_freeByteArray(engine, &bytecode->code);
    piccolo_freeIntArray(engine, &bytecode->charIdxs);
    piccolo_freeValueArray(engine, &bytecode->constants);
}

void piccolo_writeBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, uint8_t byte, int charIdx) {
    piccolo_writeByteArray(engine, &bytecode->code, byte);
    piccolo_writeIntArray(engine, &bytecode->charIdxs, charIdx);
}

void piccolo_writeConst(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, piccolo_Value constant, int charIdx) {
    int constIdx = bytecode->constants.count;

    piccolo_writeValueArray(engine, &bytecode->constants, constant);
    piccolo_writeBytecode(engine, bytecode, OP_CONST, charIdx);
    piccolo_writeBytecode(engine, bytecode, (constIdx & 0xFF00) >> 8, charIdx);
    piccolo_writeBytecode(engine, bytecode, (constIdx & 0x00FF) >> 0, charIdx);
}
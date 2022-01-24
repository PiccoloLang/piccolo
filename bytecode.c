
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

void piccolo_writeParameteredBytecode(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, uint8_t byte, uint16_t param, int charIdx) {
    piccolo_writeBytecode(engine, bytecode, byte, charIdx);
    piccolo_writeBytecode(engine, bytecode, (param & 0xFF00) >> 8, charIdx);
    piccolo_writeBytecode(engine, bytecode, (param & 0x00FF) >> 0, charIdx);
}

int piccolo_writeConst(struct piccolo_Engine* engine, struct piccolo_Bytecode* bytecode, piccolo_Value constant, int charIdx) {
    int constIdx = bytecode->constants.count;
    piccolo_writeValueArray(engine, &bytecode->constants, constant);
    piccolo_writeParameteredBytecode(engine, bytecode, PICCOLO_OP_CONST, constIdx, charIdx);
    return constIdx;
}

void piccolo_patchParam(struct piccolo_Bytecode* bytecode, int addr, uint16_t param) {
    bytecode->code.values[addr + 1] = (param & 0xFF00) >> 8;
    bytecode->code.values[addr + 2] = (param & 0x00FF) >> 0;
}

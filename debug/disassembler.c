
#include "disassembler.h"
#include <stdio.h>

static uint16_t getInstructionParam(struct piccolo_Bytecode* bytecode, int offset) {
    return (bytecode->code.values[offset + 1] << 8) + (bytecode->code.values[offset + 2] << 0);
}

int piccolo_disassembleInstruction(struct piccolo_Bytecode* bytecode, int offset) {
#define SIMPLE_INSTRUCTION(opcode)       \
    case opcode: {                       \
        printf(#opcode "\n");            \
        return offset + 1;               \
    }
#define PARAM_INSTRUCTION(opcode)        \
    case opcode: {                       \
        printf(#opcode " %d\n", getInstructionParam(bytecode, offset)); \
        return offset + 3;               \
    }

    printf("%4d | ", offset);
    switch(bytecode->code.values[offset]) {
        SIMPLE_INSTRUCTION(PICCOLO_OP_RETURN)
        case PICCOLO_OP_CONST: {
            printf("PICCOLO_OP_CONST { ");
            piccolo_printValue(bytecode->constants.values[getInstructionParam(bytecode, offset)]);
            printf(" }\n");
            return offset + 3;
        }
        SIMPLE_INSTRUCTION(PICCOLO_OP_ADD)
        SIMPLE_INSTRUCTION(PICCOLO_OP_SUB)
        SIMPLE_INSTRUCTION(PICCOLO_OP_MUL)
        SIMPLE_INSTRUCTION(PICCOLO_OP_DIV)

        SIMPLE_INSTRUCTION(PICCOLO_OP_EQUAL)
        SIMPLE_INSTRUCTION(PICCOLO_OP_GREATER)
        SIMPLE_INSTRUCTION(PICCOLO_OP_LESS)
        SIMPLE_INSTRUCTION(PICCOLO_OP_NOT)

        SIMPLE_INSTRUCTION(PICCOLO_OP_POP_STACK)

        PARAM_INSTRUCTION(PICCOLO_OP_GET_GLOBAL)
        PARAM_INSTRUCTION(PICCOLO_OP_GET_STACK)
        SIMPLE_INSTRUCTION(PICCOLO_OP_SET)

        PARAM_INSTRUCTION(PICCOLO_OP_JUMP)
        PARAM_INSTRUCTION(PICCOLO_OP_JUMP_FALSE)

        PARAM_INSTRUCTION(PICCOLO_OP_CALL)
    }
    printf("Unknown Opcode.\n");
    return offset + 1;
#undef SIMPLE_INSTRUCTION
}

void piccolo_disassembleBytecode(struct piccolo_Bytecode* bytecode) {
    int currOffset = 0;
    while(currOffset < bytecode->code.count)
        currOffset = piccolo_disassembleInstruction(bytecode, currOffset);
}

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
        SIMPLE_INSTRUCTION(OP_RETURN)
        case OP_CONST: {
            printf("OP_CONST { ");
            piccolo_printValue(bytecode->constants.values[getInstructionParam(bytecode, offset)]);
            printf(" }\n");
            return offset + 3;
        }
        SIMPLE_INSTRUCTION(OP_ADD)
        SIMPLE_INSTRUCTION(OP_SUB)
        SIMPLE_INSTRUCTION(OP_MUL)
        SIMPLE_INSTRUCTION(OP_DIV)
        SIMPLE_INSTRUCTION(OP_PRINT)
        SIMPLE_INSTRUCTION(OP_POP_STACK)

        PARAM_INSTRUCTION(OP_GET_GLOBAL)
        PARAM_INSTRUCTION(OP_GET_STACK)
        SIMPLE_INSTRUCTION(OP_SET)
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
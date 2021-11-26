
#include "disassembler.h"
#include <stdio.h>

static uint16_t getInstructionParam(struct piccolo_Bytecode* bytecode, int offset) {
    return (bytecode->code.values[offset + 1] << 8) + (bytecode->code.values[offset + 2] << 0);
}

int piccolo_disassembleInstruction(struct piccolo_Bytecode* bytecode, int offset) {
#define SIMPLE_INSTRUCTION(opcode)       \
    case PICCOLO_ ## opcode: {           \
        printf(#opcode "\n");            \
        return offset + 1;               \
    }
#define PARAM_INSTRUCTION(opcode)        \
    case PICCOLO_ ## opcode: {           \
        printf(#opcode " %d\n", getInstructionParam(bytecode, offset)); \
        return offset + 3;               \
    }

    printf("%4d | ", offset);
    switch(bytecode->code.values[offset]) {
        SIMPLE_INSTRUCTION(OP_RETURN)
        case PICCOLO_OP_CONST: {
            printf("OP_CONST { ");
            piccolo_printValue(bytecode->constants.values[getInstructionParam(bytecode, offset)]);
            printf(" }\n");
            return offset + 3;
        }
        SIMPLE_INSTRUCTION(OP_ADD)
        SIMPLE_INSTRUCTION(OP_SUB)
        SIMPLE_INSTRUCTION(OP_MUL)
        SIMPLE_INSTRUCTION(OP_DIV)
        SIMPLE_INSTRUCTION(OP_MOD)

        SIMPLE_INSTRUCTION(OP_EQUAL)
        SIMPLE_INSTRUCTION(OP_GREATER)
        SIMPLE_INSTRUCTION(OP_LESS)
        SIMPLE_INSTRUCTION(OP_NEGATE)
        SIMPLE_INSTRUCTION(OP_NOT)

        SIMPLE_INSTRUCTION(OP_POP_STACK)
        PARAM_INSTRUCTION(OP_PEEK_STACK)
        SIMPLE_INSTRUCTION(OP_SWAP_STACK)

        PARAM_INSTRUCTION(OP_CREATE_ARRAY)
        SIMPLE_INSTRUCTION(OP_CREATE_RANGE)
        SIMPLE_INSTRUCTION(OP_GET_IDX)
        SIMPLE_INSTRUCTION(OP_SET_IDX)

        SIMPLE_INSTRUCTION(OP_HASHMAP)

        PARAM_INSTRUCTION(OP_GET_GLOBAL)
        PARAM_INSTRUCTION(OP_SET_GLOBAL)
        PARAM_INSTRUCTION(OP_GET_LOCAL)
        PARAM_INSTRUCTION(OP_SET_LOCAL)

        PARAM_INSTRUCTION(OP_JUMP)
        PARAM_INSTRUCTION(OP_JUMP_FALSE)
        PARAM_INSTRUCTION(OP_REV_JUMP)
        PARAM_INSTRUCTION(OP_REV_JUMP_FALSE)

        PARAM_INSTRUCTION(OP_CALL)

        case PICCOLO_OP_CLOSURE: {
            int upvals = getInstructionParam(bytecode, offset);
            printf("OP_CLOSURE [ ");
            for(int i = 0; i < upvals; i++)
                printf("%d ", getInstructionParam(bytecode, offset + 2 + 3 * i));
            printf("]\n");
            return offset + 3 + 3 * upvals;
        }
        PARAM_INSTRUCTION(OP_GET_UPVAL)
        PARAM_INSTRUCTION(OP_SET_UPVAL)
        SIMPLE_INSTRUCTION(OP_CLOSE_UPVALS)

        SIMPLE_INSTRUCTION(OP_GET_LEN)
        SIMPLE_INSTRUCTION(OP_APPEND)

        SIMPLE_INSTRUCTION(OP_EXECUTE_PACKAGE)
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

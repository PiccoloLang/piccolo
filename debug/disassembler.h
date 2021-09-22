
#ifndef PICCOLO_DISASSEMBLER_H
#define PICCOLO_DISASSEMBLER_H

#include "../bytecode.h"

int piccolo_disassembleInstruction(struct piccolo_Bytecode* bytecode, int offset);
void piccolo_disassembleBytecode(struct piccolo_Bytecode* bytecode);

#endif

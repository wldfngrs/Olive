#include <stdio.h>

#include "debug.h"
#include "value.h"

void resetDebugInfo() {
	currentLine = 0;
	operationsPerLine = 0;
	indx = 0;
	temp;	
}

static int simpleInstruction(const char* name, int offset) {
	printf("%s\n", name);
	return offset + 1;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
	uint8_t constantIndex = chunk->code[offset + 1];
	printf("%-16s %14d '", name, constantIndex);
	printValue(chunk->constants.values[constantIndex]);
	printf("'\n");
	return offset + 2;
}

static int constantLongInstruction(const char* name, Chunk* chunk, int offset) {
	uint8_t constantIndex = (chunk->code[offset + 1] | (chunk->code[offset + 2] << 8) | (chunk->code[offset + 3] << 16));
	printf("%-16s %14d '", name, constantIndex);
	printValue(chunk->constants.values[constantIndex]);
	printf("'\n");
	return offset + 4;
}

void disassembleChunk(Chunk* chunk, const char* name) {
	printf("== %s ==\n", name);
	
	for (int offset = 0; offset < chunk->count;) {
		offset = disassembleInstruction(chunk, offset);
	}
}

int disassembleInstruction(Chunk* chunk, int offset) {
	printf("%04d ", offset);
	
	if (offset > 0 && getLine(chunk, offset) == getLine(chunk, offset - 1)) {
		printf("   | ");
	} else {
		printf("%4d ", getLine(chunk, offset));
	}
	
	uint8_t instruction = chunk->code[offset];
	switch(instruction) {
		case OP_CONSTANT_LONG:
			return constantLongInstruction("OP_CONSTANT_LONG", chunk, offset);
		case OP_CONSTANT:
			return constantInstruction("OP_CONSTANT", chunk, offset);
		case OP_ADD:
			return simpleInstruction("OP_ADD", offset);
		case OP_SUBTRACT:
			return simpleInstruction("OP_SUBTRACT", offset);
		case OP_MULTIPLY:
			return simpleInstruction("OP_MULTIPLY", offset);
		case OP_DIVIDE:
			return simpleInstruction("OP_DIVIDE", offset);
		case OP_NEGATE:
			return simpleInstruction("OP_NEGATE", offset);
		case OP_RETURN:
			return simpleInstruction("OP_RETURN", offset);
		default:
			printf("Unknown opcode %d\n", instruction);
			return offset + 1;
	}
}

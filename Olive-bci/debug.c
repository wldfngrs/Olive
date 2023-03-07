#include <stdio.h>

#include "debug.h"
#include "value.h"

static int simpleInstruction(const char* name, int offset) {
	printf("%s\n", name);
	return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
	uint8_t slot = chunk->code[offset + 1];
	printf("%-16s %14d\n", name, slot);
	return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
	uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
	jump |= chunk->code[offset+2];
	printf("%-16s %14d -> %d\n", name, offset, offset + 3 + sign * jump);
	return offset + 3;
}

static int popNInstruction(const char* name, Chunk* chunk, int offset) {
	uint8_t popCount = chunk->code[offset + 1];
	printf("%-16s %14d\n", name, popCount);
	return offset + 2;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
	uint8_t constantIndex = chunk->code[offset + 1];
	printf("%-16s %14d '", name, constantIndex);
	printValue(chunk->constants->values[constantIndex]);
	printf("'\n");
	return offset + 2;
}

static int constantLongInstruction(const char* name, Chunk* chunk, int offset) {
	uint32_t constantIndex = (chunk->code[offset + 1] | (chunk->code[offset + 2] << 8) | (chunk->code[offset + 3] << 16));
	printf("%-16s %14d '", name, constantIndex);
	printValue(chunk->constants->values[constantIndex]);
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
		case OP_NULL:
			return simpleInstruction("OP_NULL", offset);
		case OP_TRUE:
			return simpleInstruction("OP_TRUE", offset);
		case OP_FALSE:
			return simpleInstruction("OP_FALSE", offset);
		case OP_POP:
			return simpleInstruction("OP_POP", offset);
		case OP_POPN:
			return popNInstruction("OP_POPN", chunk, offset);
		case OP_GET_LOCAL:
			return byteInstruction("OP_GET_LOCAL", chunk, offset);
		case OP_SET_LOCAL:
			return byteInstruction("OP_SET_LOCAL", chunk, offset); 
		case OP_GET_GLOBAL: {
			if (chunk->constants->count < 256) {
				return constantInstruction("OP_GET_GLOBAL", chunk, offset);	
			}
			return constantLongInstruction("OP_GET_GLOBAL", chunk, offset);
		}
		case OP_DEFINE_GLOBAL:
			if (chunk->constants->count < 256) {
				return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);;	
			}
			return constantLongInstruction("OP_DEFINE_GLOBAL", chunk, offset);
		case OP_SET_GLOBAL:
			if (chunk->constants->count < 256) {
				return constantInstruction("OP_SET_GLOBAL", chunk, offset);;	
			}
			return constantLongInstruction("OP_SET_GLOBAL", chunk, offset);
		case OP_EQUAL:
			return simpleInstruction("OP_EQUAL", offset);
		case OP_SWITCH_EQUAL:
			return simpleInstruction("OP_SWITCH_EQUAL", offset);
		case OP_NOT_EQUAL:
			return simpleInstruction("OP_NOT_EQUAL", offset);
		case OP_GREATER:
			return simpleInstruction("OP_GREATER", offset);
		case OP_GREATER_EQUAL:
			return simpleInstruction("OP_GREATER_EQUAL", offset);
		case OP_LESS:
			return simpleInstruction("OP_LESS", offset);
		case OP_LESS_EQUAL:
			return simpleInstruction("OP_LESS_EQUAL", offset);
		case OP_TERNARY:
			return simpleInstruction("OP_TERNARY", offset);
		case OP_ADD:
			return simpleInstruction("OP_ADD", offset);
		case OP_SUBTRACT:
			return simpleInstruction("OP_SUBTRACT", offset);
		case OP_MULTIPLY:
			return simpleInstruction("OP_MULTIPLY", offset);
		case OP_DIVIDE:
			return simpleInstruction("OP_DIVIDE", offset);
		case OP_NOT:
			return simpleInstruction("OP_NOT", offset);
		case OP_NEGATE:
			return simpleInstruction("OP_NEGATE", offset);
		case OP_PRINT:
			return simpleInstruction("OP_PRINT", offset);
		case OP_JUMP:
			return jumpInstruction("OP_JUMP", 1, chunk, offset);
		case OP_JUMP_IF_FALSE:
			return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
		case OP_LOOP:
			return jumpInstruction("OP_LOOP", -1, chunk, offset);
		case OP_CONTINUE:
			return jumpInstruction("OP_CONTINUE", -1, chunk, offset);
		/*case OP_BREAK:
			return simpleInstruction("OP_BREAK", offset);
		case OP_JUMP_IF_BREAK:
			return jumpInstruction("OP_JUMP_IF_BREAK", 1, chunk, offset);*/
		case OP_BREAK:
			return jumpInstruction("OP_BREAK", 1, chunk, offset);
		case OP_FALLTHROUGH:
			return simpleInstruction("OP_FALLTHROUGH", offset);
		case OP_CALL:
			return byteInstruction("OP_CALL", chunk, offset);
		case OP_RETURN:
			return simpleInstruction("OP_RETURN", offset);
		default:
			printf("Unknown opcode %d\n", instruction);
			return offset + 1;
	}
}

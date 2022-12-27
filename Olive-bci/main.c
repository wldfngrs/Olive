#include "common.h"
#include "chunk.h"
#include "debug.h"
#include <stdio.h>

int main(int argc, const char* argv[]) {
	Chunk chunk;
	initChunk(&chunk);
	int constant = addConstant(&chunk, 1.2);
	writeChunk(&chunk, OP_CONSTANT, 123);
	writeChunk(&chunk, constant, 123);
	writeConstant(&chunk, 3.4, 125);
	writeChunk(&chunk, OP_RETURN, 126);

	disassembleChunk(&chunk, "test chunk");

	freeChunk(&chunk);
	return 0;
}

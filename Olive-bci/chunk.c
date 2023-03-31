#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"

/*	Clear debug line info. If the debug flags are uncommented in 'common.h' this function helps to clear any existing	 
line info in the case of a parsing error. */
void clearLineInfo() {
	currentLine = 0;
	operationsPerLine = 0;
	indx = 0;
}

/* Initialize a chunk to hold bytecode. */
void initChunk(Chunk* chunk, ValueArray* constants) {
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lineArr = NULL;
	chunk->codeArr = NULL;
	//initValueArray(&chunk->constants);
	chunk->constants = constants;
}

/* Free chunk containing bytecode. */
void freeChunk(Chunk* chunk) {
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(int, chunk->codeArr, chunk->capacity);
	FREE_ARRAY(int, chunk->lineArr, chunk->capacity);
	freeValueArray(chunk->constants);
	initChunk(chunk, chunk->constants);
}

/* For REPL persistence. Free's the chunk with an error but preserves the stored value array. That way values declared before the error are preserved. */
void freeChunkButNotValueArray(Chunk* chunk) {
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(int, chunk->codeArr, chunk->capacity);
	FREE_ARRAY(int, chunk->lineArr, chunk->capacity);
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lineArr = NULL;
	chunk->codeArr = NULL;
}

/* Write a byte to a chunk. */
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
	if(chunk->capacity < chunk->count + 1) {
		int oldCapacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(oldCapacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
		chunk->lineArr = GROW_ARRAY(int, chunk->lineArr, oldCapacity, chunk->capacity);
		chunk->codeArr = GROW_ARRAY(int, chunk->codeArr, oldCapacity, chunk->capacity);
	}
	
	if (line != currentLine) {
		currentLine = line;
		operationsPerLine = 1;
		chunk->lineArr[indx] = line;
		temp = indx;
		chunk->codeArr[temp] = operationsPerLine;
		indx++; 
	} else {
		chunk->codeArr[temp] = ++operationsPerLine;
	}
	
	chunk->code[chunk->count] = byte;
	chunk->count++;
}

/* Add a 'Value' constant to the constant array 'ValueArray'. */
int addConstant(Chunk* chunk, Value value, bool isConst) {
	value.isConst = isConst;
	push(value);
	writeValueArray(chunk->constants, value);
	pop(1);
	return chunk->constants->count - 1;
}

/* Combination of the addConstant() and writeChunk() functions. */
void writeConstant(Chunk* chunk, Value value, int line) {
	int constantIndex = addConstant(chunk, value, false);
	
	if (constantIndex < 256) {
		writeChunk(chunk, OP_CONSTANT, line);
		writeChunk(chunk, (uint8_t)constantIndex, line);
	} else {
		writeChunk(chunk, OP_CONSTANT_LONG, line);
		writeChunk(chunk, (uint8_t)(constantIndex & 0xff), line);
		writeChunk(chunk, (uint8_t)((constantIndex >> 8) & 0xff), line);
		writeChunk(chunk, (uint8_t)((constantIndex >> 16) & 0xff), line);
	}
}

/* Returns the current line of execution for debug and error handling. */
int getLine(Chunk* chunk, int instructionIndex) {
	int sum = 0;
	int i;
	for (i = 0; sum <= instructionIndex; i++ ) {
		sum += chunk->codeArr[i];
	}
	
	return chunk->lineArr[--i];
}

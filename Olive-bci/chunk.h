#ifndef olive_chunk_h
#define olive_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
	OP_CONSTANT_LONG,
	OP_CONSTANT,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,	
	OP_NEGATE,
	OP_RETURN,
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t* code;
	//int* lines;
	int* lineArr;
	int* codeArr;
	ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t bytes, int line);
int addConstant(Chunk* chunk, Value value);
void writeConstant(Chunk* chunk, Value value, int line);
int getLine(Chunk* chunk, int instructionIndex);

#endif

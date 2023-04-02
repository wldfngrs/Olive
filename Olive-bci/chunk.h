#ifndef olive_chunk_h
#define olive_chunk_h

#include "common.h"
#include "value.h"

/* Line info cleared by clearLineInfo() function at parsing error. */
static int currentLine = 0;
static int operationsPerLine = 0;
static int indx = 0;
static int temp;

/* Bytecode instructions-set. */
typedef enum {
	OP_CONSTANT_LONG,
	OP_CONSTANT,
	OP_NULL,
	OP_TRUE,
	OP_FALSE,
	OP_POP,
	OP_POPN,
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_GET_GLOBAL,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,
	OP_GET_PROPERTY,
	OP_SET_PROPERTY,
	OP_GET_BASE,
	OP_DELATTR,
	OP_DEFINE_GLOBAL,
	OP_SET_GLOBAL,
	OP_EQUAL,
	OP_SWITCH_EQUAL,
	OP_NOT_EQUAL,
	OP_GREATER,
	OP_GREATER_EQUAL,
	OP_LESS,
	OP_LESS_EQUAL,
	OP_TERNARY,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_MOD,
	OP_NOT,	
	OP_NEGATE,
	OP_PRINT,
	OP_JUMP,
	OP_JUMP_IF_FALSE,
	OP_LOOP,
	OP_CALL,
	OP_CLOSURE,
	OP_CLOSE_UPVALUE,
	OP_BREAK,
	//OP_JUMP_IF_BREAK,
	OP_FALLTHROUGH,
	OP_CONTINUE,
	OP_RETURN,
	OP_CLASS,
	OP_INHERIT,
	OP_INVOKE,
	OP_BASE_INVOKE,
	OP_METHOD,
} OpCode;

/* A Chunk type to hold the bytecode instructions. A dynamic array with the ValueArray included in it's definition. */
typedef struct {
	int count;
	int capacity;
	uint8_t* code;
	int* lineArr;
	int* codeArr;
	ValueArray* constants;
} Chunk;

void initChunk(Chunk* chunk, ValueArray* constants);
void freeChunk(Chunk* chunk);
void freeChunkButNotValueArray(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t bytes, int line);
int addConstant(Chunk* chunk, Value value, bool constness);
void writeConstant(Chunk* chunk, Value value, int line);
int getLine(Chunk* chunk, int instructionIndex);
void clearLineInfo();

#endif

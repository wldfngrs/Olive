#ifndef olive_vm_h
#define olive_vm_h

#include "chunk.h"
#include "stack.h"
#include "table.h"

#define STACK_MAX 256

typedef struct {
	Chunk* chunk;
	uint8_t* ip;
	Stack stack;
	Value* stackTop;
	Table globals;
	Table strings;
	Table globalConstantIndex; // probably find a better name
	Obj* objects;
} VM;


typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
InterpretResult interpretREPL(const char* source);
void push(Value value);
Value pop(uint8_t popCount);

#endif

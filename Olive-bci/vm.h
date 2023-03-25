#ifndef olive_vm_h
#define olive_vm_h

#include "chunk.h"
#include "stack.h"
#include "object.h"
#include "table.h"

#define FRAMES_MAX 64

#define STACK_MAX 256
#define NATIVE_ID_MAX 10

typedef struct {
	ObjClosure* closure;
	uint8_t* ip;
	Value* slots;
} CallFrame;

typedef struct {
	//Chunk* chunk;
	//uint8_t* ip;
	CallFrame frames[FRAMES_MAX];
	int frameCount;
	
	Stack stack;
	Value* stackTop;
	Table globals;
	Table strings;
	ObjString* initString;
	Table globalConstantIndex; // probably find a better name
	int nativeIdentifierCount;
	const char* nativeIdentifiers[NATIVE_ID_MAX];
	ObjUpvalue* openUpvalues;
	Obj* objects;
	
	size_t bytesAllocated;
	size_t nextGC;
	
	int grayCount;
	int grayCapacity;
	Obj** grayStack;
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

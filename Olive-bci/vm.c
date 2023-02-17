#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

VM vm;
Chunk chunkREPL;

static void resetStack() {
	initStack(&vm.stack);
	vm.stackTop = vm.stack.stack;
}

static void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);
	
	size_t instruction = vm.ip - vm.chunk->code - 1;
	int line = getLine(vm.chunk, instruction);
	fprintf(stderr, "[line %d] in script\n", line);
	
	resetStack();
}

void initVM() {
	resetStack();
	vm.objects = NULL;
	initTable(&vm.globals);
	initTable(&vm.strings);
}

void freeVM() {
	freeTable(&vm.globals);
	freeTable(&vm.strings);
	if (REPLmode && vm.chunk != NULL) {
		freeValueArray(&vm.chunk->constants);
	}
	freeStack(&vm.stack);
	freeObjects();
}

void push(Value value) {
	if(vm.stack.capacity <  vm.stack.count + 1) {
		growStack(&vm.stack);
	}
	
	*vm.stackTop = value;
	vm.stackTop++;
}

Value pop() {
	vm.stackTop--;
	return *vm.stackTop;
}

static Value peek(int distance) {
	return vm.stackTop[-1-distance];
}

static bool isFalsey(Value value) {
	return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
	//concatenation won't happen too often so I guess it's okay to let push and pop go crazy in here
	ObjString* b = AS_STRING(pop());
	ObjString* a = AS_STRING(pop());
	
	int length = a->length + b->length;
	char* chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	
	ObjString* result = takeString(chars, length);
	push(OBJ_VAL(result));
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_LONG_CONSTANT() (vm.chunk->constants.values[vm.chunk->code[(int)(vm.ip - vm.chunk->code)+1] |\
			      vm.chunk->code[(int)(vm.ip - vm.chunk->code)+2] << 8 |\
			      vm.chunk->code[(int)(vm.ip - vm.chunk->code)+3] << 16])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op)\
	do { \
		if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {\
			runtimeError("Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		}\
		double b = AS_NUMBER(pop()); \
		Value* stackTop = vm.stackTop - 1; \
		AS_NUMBER(*stackTop) = AS_NUMBER(*stackTop) op b; \
	} while (false)

	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("          ");
		for (Value* slot = vm.stack.stack; slot < vm.stackTop; slot++) {
			printf("[ ");
			printValue(*slot);
			printf(" ]");
		}
		printf("\n");
		disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
		uint8_t instruction;
		switch(instruction = READ_BYTE()) {
			case OP_CONSTANT: {
				Value constant = READ_CONSTANT();
				push(constant);
				break;
			}
			case OP_CONSTANT_LONG: {
				Value constant = READ_LONG_CONSTANT();
				push(constant);
				break;
			}
			case OP_NULL: push(NULL_VAL); break;
			case OP_TRUE: push(BOOL_VAL(true)); break;
			case OP_FALSE: push(BOOL_VAL(false)); break;
			case OP_POP: pop(); break;
			case OP_GET_GLOBAL: {
				ObjString* name = READ_STRING();
				Value value;
				if (!tableGet(&vm.globals, &OBJ_KEY(name), &value)) {
					runtimeError("Undefined variable '%.*s'.", name->length, name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				push(value);
				break;
			}
			case OP_DEFINE_GLOBAL:
				ObjString* name = READ_STRING();
				tableSet(&vm.globals, &OBJ_KEY(name), peek(0));
				pop();
				break;
			case OP_EQUAL:
				Value b = pop();
				Value* aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesEqual(*aPtr, b));
				break;
			case OP_NOT_EQUAL:
				b = pop();
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesNotEqual(*aPtr, b));
				break;
			// Make these work for non-number types as well
			case OP_GREATER:
				b = pop();
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesGreater(*aPtr, b));
				break;
			case OP_GREATER_EQUAL:
				b = pop();
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesGreaterEqual(*aPtr, b));
				break;
			case OP_LESS:
				b = pop();
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesLess(*aPtr, b));
				break;
			case OP_LESS_EQUAL:
				b = pop();
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesLessEqual(*aPtr, b));
				break;
			case OP_TERNARY:
				b = pop();
				Value a = pop();
				Value* conditional = vm.stackTop - 1;
				*conditional = AS_BOOL(*conditional) ? a : b;
				break;
			case OP_ADD: {
				if(IS_STRING(peek(0)) && IS_STRING(peek(1))) {
					concatenate();
				} else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
					double b = AS_NUMBER(pop());
					Value* stackTop = vm.stackTop - 1; 
		AS_NUMBER(*stackTop) = AS_NUMBER(*stackTop)+ b;
				} else {
					runtimeError("Operands must be two numbers or two strings.");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
			case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
			case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
			case OP_NOT:
				push(BOOL_VAL(isFalsey(pop())));
				break;
			case OP_NEGATE: {
				if(!IS_NUMBER(peek(0))) {
					runtimeError("Operand must be a number.");
					return INTERPRET_RUNTIME_ERROR;
				}
				
				// Check here for errors
				Value* valueToNegate = vm.stackTop - 1;
				
				(AS_NUMBER(*valueToNegate)) = 0 - (AS_NUMBER(*valueToNegate)); break;
			}
			case OP_PRINT: {
				printValue(pop());
				printf("\n");
				break;
			}
			case OP_RETURN: {
				return INTERPRET_OK;
			}
		}
	}
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_LONG_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
	Chunk chunk;
	
	initChunk(&chunk);
	
	if (!compile(source, &chunk)) {
		freeChunk(&chunk);
		clearLineInfo();
		return INTERPRET_COMPILE_ERROR;
	}
	
	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;
	
	InterpretResult result = run();
	
	clearLineInfo();
	freeChunk(&chunk);
}

bool withinREPL = false;

InterpretResult interpretREPL(const char* source) {
	if (!withinREPL) initChunk(&chunkREPL);
	
	if (!compile(source + prevLength, &chunkREPL)) {
		freeChunkButNotValueArray(&chunkREPL);
		clearLineInfo();
		return INTERPRET_COMPILE_ERROR;
	}
	
	vm.chunk = &chunkREPL;
	vm.ip = vm.chunk->code;
	
	InterpretResult result = run();
	clearLineInfo();
	freeChunkButNotValueArray(&chunkREPL);
	withinREPL = true;
}

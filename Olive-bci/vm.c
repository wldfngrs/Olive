#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

VM vm;


bool switchFallThrough = false;

static void resetStack() {
	initStack(&vm.stack);
	vm.stackTop = vm.stack.stack;
	vm.frameCount = 0;
}

static void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);
	
	for (int i = vm.frameCount - 1; i >= 0; i--) {
		CallFrame* frame = &vm.frames[i];
		ObjFunction* function = frame->function;
		// -1 because the instruction pointer is sitting on the next instruction to be executed.
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", getLine(&frame->function->chunk, instruction));
		if (function->name == NULL) {
			fprintf(stderr, "script\n");
		} else {
			fprintf(stderr, "%.*s()\n", function->name->length, function->name->chars);
		}
	}
	
	resetStack();
}

static Value clockNative(int argCount, Value* args) {
	if (argCount != 0) {
		runtimeError("'clock' function call expected 0 argument(s). Initialized with %d argument(s) instead.", argCount);
		return NULL_VAL;
	}
	
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void defineNative(const char* name, NativeFunction function) {
	Value functionName = OBJ_VAL(allocateString(false, name, (int)strlen(name)));
	Value functionCall = OBJ_VAL(newNative(function));
	push(functionName);
	push(functionCall);
	tableSet(&vm.globals, &OBJ_KEY(AS_STRING(vm.stack.stack[0])), vm.stack.stack[1]);
	pop(2);
	vm.nativeIdentifiers[vm.nativeIdentifierCount++] = name;
}

void initVM() {
	resetStack();
	vm.objects = NULL;
	initTable(&vm.globals);
	initTable(&vm.strings);
	initTable(&vm.globalConstantIndex);
	vm.nativeIdentifierCount = 0;
	
	defineNative("clock", clockNative);
}

void freeVM() {
	freeTable(&vm.globals);
	freeTable(&vm.globalConstantIndex);
	freeTable(&vm.strings);
	if (REPLmode && &vm.frames[0].function->chunk != NULL) {
		freeValueArray(vm.frames[0].function->chunk.constants);
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
	vm.stack.count++;
}

Value pop(uint8_t popCount) {
	if (vm.stack.count == 0) {
		return *vm.stackTop;
	}
	
	vm.stackTop -= popCount;
	vm.stack.count -= popCount;
	return *vm.stackTop;
}

static Value peek(int distance) {
	return vm.stackTop[-1-distance];
}

static bool call(ObjFunction* function, int argCount) {
	if (argCount != function->arity) {
		runtimeError("'%.*s' function call expected %d argument(s). Initialized with %d argument(s) instead.", function->name->length, function->name->chars, function->arity, argCount);
		return false;
	}
	
	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow. :)");
		return false;
	}

	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->function = function;
	frame->ip = function->chunk.code;
	
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

static bool callValue(Value callee, int argCount) {
	if(IS_OBJ(callee)) {
		switch(OBJ_TYPE(callee)) {
			case OBJ_FUNCTION:
				return call(AS_FUNCTION(callee), argCount);
			
			case OBJ_NATIVE: {
				NativeFunction native = AS_NATIVE(callee);
				Value result = native(argCount, vm.stackTop - argCount);
				if (IS_NULL(result)) {
					return false;
				}
				
				vm.stackTop -= argCount + 1;
				push(result);
				return true;
			}	
			
			default:
				// non-callable object type.
				break;
		}
	}
	
	runtimeError("Non-callable object type.");
	return false;
}

static bool isFalsey(Value value) {
	return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static bool isTruthy(Value value) {
	return IS_NULL(value) || (IS_BOOL(value) && AS_BOOL(value));
}

static void concatenate() {
	//concatenation won't happen too often so I guess it's okay to let push and pop go crazy in here
	ObjString* b = AS_STRING(pop(1));
	ObjString* a = AS_STRING(pop(1));
	
	int length = a->length + b->length;
	char* chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	
	ObjString* result = takeString(chars, length);
	push(OBJ_VAL(result));
}

static InterpretResult run() {
	CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->function->chunk.constants->values[READ_BYTE()])
#define READ_LONG_CONSTANT() (frame->function->chunk.constants->values[frame->function->chunk.code[(int)(frame->ip - frame->function->chunk.code)+1] |\
			      frame->function->chunk.code[(int)(frame->ip - frame->function->chunk.code)+2] << 8 |\
			      frame->function->chunk.code[(int)(frame->ip - frame->function->chunk.code)+3] << 16])
#define READ_SHORT() \
	(frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op)\
	do { \
		if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {\
			runtimeError("Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		}\
		double b = AS_NUMBER(pop(1)); \
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
		disassembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
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
			case OP_POP: pop(1); break;
			case OP_POPN: {
				uint8_t popCount = READ_BYTE();
				pop(popCount);
				break;
			}
			case OP_GET_LOCAL: {
				uint8_t slot = READ_BYTE();
				push(frame->slots[slot]);
				break;
			}
			case OP_SET_LOCAL: {
				uint8_t slot = READ_BYTE();
				frame->slots[slot] = peek(0);
				break;
			}
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
				pop(1);
				break;
			case OP_SET_GLOBAL:{
				ObjString* name = READ_STRING();
				if (tableSet(&vm.globals, &OBJ_KEY(name), peek(0))) {
					tableDelete(&vm.globals, &OBJ_KEY(name));
					runtimeError("Undefined variable '%.*s'.", name->length, name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_EQUAL:
				Value b = pop(1);
				Value* aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesEqual(*aPtr, b));
				break;
			case OP_SWITCH_EQUAL:
				aPtr = vm.stackTop - 1;
				if (switchFallThrough) {
					*aPtr = BOOL_VAL(true);
					switchFallThrough = false;
				} else {
					*aPtr = BOOL_VAL(valuesEqual(*aPtr, *(aPtr - 1)));
				}
				break;
			case OP_NOT_EQUAL:
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesNotEqual(*aPtr, b));
				break;
			// Make these work for non-number types as well
			case OP_GREATER:
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesGreater(*aPtr, b));
				break;
			case OP_GREATER_EQUAL:
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesGreaterEqual(*aPtr, b));
				break;
			case OP_LESS:
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesLess(*aPtr, b));
				break;
			case OP_LESS_EQUAL:
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesLessEqual(*aPtr, b));
				break;
			case OP_TERNARY:
				b = pop(1);
				Value a = pop(1);
				Value* conditional = vm.stackTop - 1;
				*conditional = AS_BOOL(*conditional) ? a : b;
				break;
			case OP_ADD: {
				if(IS_STRING(peek(0)) && IS_STRING(peek(1))) {
					concatenate();
				} else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
					double b = AS_NUMBER(pop(1));
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
				push(BOOL_VAL(isFalsey(pop(1))));
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
				printValue(pop(1));
				//if (REPLmode) printf("\n");
				printf("\n");
				break;
			}
			case OP_JUMP: {
				uint16_t offset = READ_SHORT();
				frame->ip += offset;
				break;
			}
			case OP_JUMP_IF_FALSE: {
				uint8_t offset = READ_SHORT();
				if(isFalsey(peek(0))) frame->ip += offset;
				break;
			}
			case OP_LOOP: {
				uint16_t offset = READ_SHORT();
				frame->ip -= offset;
				break;
			}
			
			case OP_CONTINUE: {
				uint16_t offset = READ_SHORT();
				frame->ip -= offset;
				break;
			}
			/*case OP_BREAK: {
				Value breakVal;
				breakVal.type = VAL_BREAK;
				push(breakVal);
				break;
			}
			case OP_JUMP_IF_BREAK: {
				uint16_t offset = READ_SHORT();
				if(peek(0).type == VAL_BREAK) {
					vm.ip += offset;
				}
				
				break;
			}*/
			
			case OP_CALL: {
				int argCount = READ_BYTE();
				if (!callValue(peek(argCount), argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}
			
			case OP_BREAK: {
				uint16_t offset = READ_SHORT();
				frame->ip += offset;
				break;
			}
			
			case OP_FALLTHROUGH: {
				switchFallThrough = true;
				break;
			}
			
			case OP_RETURN: {
				Value result = pop(1);
				
				vm.frameCount--;
				if (vm.frameCount == 0) {
					pop(1);
					return INTERPRET_OK;
				}
				
				vm.stackTop = frame->slots;
				push(result);
				
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}
		}
	}
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_LONG_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
	ObjFunction* function = compile(source);
	if (function == NULL) {
		clearLineInfo();
		return INTERPRET_COMPILE_ERROR;
	}
	
	push(OBJ_VAL(function));
	callValue(OBJ_VAL(function), 0);
	
	InterpretResult result = run();
	clearLineInfo();
	return result;
}

bool withinREPL = false;

Chunk chunkREPL;

InterpretResult interpretREPL(const char* source) {
	ObjFunction* function = compileREPL(source);
	if (function == NULL) {
		clearLineInfo();
		return INTERPRET_COMPILE_ERROR;
	}
	
	push(OBJ_VAL(function));
	callValue(OBJ_VAL(function), 0);
	
	InterpretResult result = run();
	clearLineInfo();
	withinREPL = true;
	return result;	

	/*if (!withinREPL) initChunk(&chunkREPL);
	
	if (!compile(source, &chunkREPL)) {
		freeChunkButNotValueArray(&chunkREPL);
		clearLineInfo();
		return INTERPRET_COMPILE_ERROR;
	}
	
	vm.chunk = &chunkREPL;
	vm.ip = vm.chunk->code;
	
	InterpretResult result = run();
	clearLineInfo();
	freeChunkButNotValueArray(&chunkREPL);
	withinREPL = true;*/
}

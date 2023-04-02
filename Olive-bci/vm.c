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
	vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	//fputs("\n", stderr);
	
	for (int i = vm.frameCount - 1; i >= 0; i--) {
		CallFrame* frame = &vm.frames[i];
		ObjFunction* function = frame->closure->function;
		// -1 because the instruction pointer is sitting on the next instruction to be executed.
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", getLine(&frame->closure->function->chunk, instruction));
		if (function->name == NULL) {
			fprintf(stderr, "script\n\e[0m");
		} else {
			fprintf(stderr, "%.*s()\n\e[0m", function->name->length, function->name->chars);
		}
	}
	
	resetStack();
}

static Value clockNative(int argCount, Value* args) {
	if (argCount != 0) {
		runtimeError("\e[1;31mError: 'clock' function call expected 0 argument(s). Initialized with %d argument(s) instead, ", argCount);
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
	
	vm.bytesAllocated = 0;
	vm.nextGC = 1024 * 1024;
	
	vm.grayCount = 0;
	vm.grayCapacity = 0;
	vm.grayStack = NULL;
	
	initTable(&vm.globals);
	
	initTable(&vm.strings);
	vm.initString = NULL;
	vm.initString = allocateString(false, "init", 4);
	
	initTable(&vm.globalConstantIndex);
	
	vm.nativeIdentifierCount = 0;
	
	defineNative("clock", clockNative);
}

void freeVM(bool REPLmode) {
	freeTable(&vm.globals);
	freeTable(&vm.globalConstantIndex);
	freeTable(&vm.strings);
	vm.initString = NULL;
	if (REPLmode && vm.frameCount != 0) {
		freeValueArray(vm.frames[0].closure->function->chunk.constants);
	}
	freeStack(&vm.stack);
	freeObjects();
}

void push(Value value) {
	if(vm.stack.capacity <  vm.stack.count + 1) {
		int temp = vm.stackTop - vm.stack.stack;
		growStack(&vm.stack);
		vm.stackTop = vm.stack.stack + temp;
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

static bool call(ObjClosure* closure, int argCount) {
	if (argCount != closure->function->arity) {
		runtimeError("\e[1;31mError: '%.*s' function call expected %d argument(s). Initialized with %d argument(s) instead, ", closure->function->name->length, closure->function->name->chars, closure->function->arity, argCount);
		return false;
	}
	
	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("\e[1;31mError: Stack overflow. :), ");
		return false;
	}

	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

static bool callValue(Value callee, int argCount) {
	if(IS_OBJ(callee)) {
		switch(OBJ_TYPE(callee)) {
			case OBJ_BOUND_METHOD: {
				ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
				vm.stackTop[-argCount - 1] = bound->reciever;
				return call(bound->method, argCount);
			}
			
			case OBJ_CLASS: {
				ObjClass* c = AS_CLASS(callee);
				vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(c));
				Value initializer;
				if (!IS_NULL(c->initCall)) {
					return call(AS_CLOSURE(c->initCall), argCount);	
				} else if (tableGet(&c->methods, &OBJ_KEY(vm.initString), &initializer)) {
					return call(AS_CLOSURE(initializer), argCount);
				} else if (argCount != 0) {
					runtimeError("\e[1;31mError: Expected 0 arguments but got %d, ");
					return false;
				}
				
				return true;
			}
			
			case OBJ_CLOSURE:
				return call(AS_CLOSURE(callee), argCount);
			
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
	
	runtimeError("\e[1;31mError: Non-callable object type, ");
	return false;
}

static bool invokeFromClass(ObjClass* c, ObjString* name, int argCount) {
	Value method;
	if (!tableGet(&c->methods, &OBJ_KEY(name), &method)) {
		runtimeError("\e[1;31mUndefined property '%.*s', ", name->length, name->chars);
		return false;
	}
	
	return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
	Value reciever = peek(argCount);
	
	if (!IS_INSTANCE(reciever)) {
		runtimeError("\e[1;31mError: Attempt to call method on non-instance, ");
		return false;
	}
	
	ObjInstance* instance = AS_INSTANCE(reciever);
	
	Value value;
	if (tableGet(&instance->fields, &OBJ_KEY(name), &value)) {
		vm.stackTop[-argCount - 1] = value;
		return callValue(value, argCount);
	}
	
	return invokeFromClass(instance->c, name, argCount);
}

static bool bindMethod(ObjClass* c, ObjString* name) {
	Value method;
	if(!tableGet(&c->methods, &OBJ_KEY(name), &method)) {
		runtimeError("\e[1;31mError: Undefined property '%.*s', ", name->length, name->chars);
		return false;
	}
	
	ObjBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method));
	
	pop(1);
	push(OBJ_VAL(bound));
	return true;
}

static ObjUpvalue* captureUpValue(Value* local) {
	ObjUpvalue* prevUpvalue = NULL;
	ObjUpvalue* upvalue = vm.openUpvalues;
	
	while (upvalue != NULL && upvalue->location > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;	
	}
	
	if (upvalue != NULL && upvalue->location == local) {
		return upvalue;
	}
	
	ObjUpvalue* createdUpvalue = newUpvalue(local);
	createdUpvalue->next = upvalue;
	
	if (prevUpvalue == NULL) {
		vm.openUpvalues = createdUpvalue;
	} else {
		prevUpvalue->next = createdUpvalue;
	}
	
	return createdUpvalue;
}

static void closeUpvalues(Value* last) {
	while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
		ObjUpvalue* upvalue = vm.openUpvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm.openUpvalues = upvalue->next;
	}
}

static void defineMethod(ObjString* name) {
	Value method = peek(0);
	ObjClass* c = AS_CLASS(peek(1));
	tableSet(&c->methods, &OBJ_KEY(name), method);
	if (name == vm.initString) {
		c->initCall = method;
	}
	
	pop(1);
}

static bool isFalsey(Value value) {
	return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

/*static bool isTruthy(Value value) {
	return IS_NULL(value) || (IS_BOOL(value) && AS_BOOL(value));
}*/

static void concatenate() {
	ObjString* b = AS_STRING(peek(0));
	ObjString* a = AS_STRING(peek(1));
	
	int length = a->length + b->length;
	char* chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	
	ObjString* result = takeString(chars, length);
	pop(2);
	push(OBJ_VAL(result));
}

static void convconcatenate() {
	Value a = peek(0);
	Value b = peek(1);
	
	int length = 0;
	char* result = ALLOCATE(char, 1024); // not a great use of memory, sigh, I know
	
	if (result == NULL) {
		runtimeError("\e[1;31mError: Unexpected memory allocation error. ");
	}
	
	switch(b.type) {
		case VAL_BOOL: {
			bool bl = AS_BOOL(b);
			if (bl == true) {
				memcpy(result+length, "true", 4);
				length += 4;
			} else {
				memcpy(result+length, "false", 5);
				length += 5;
			}
			break;
		}
		
		case VAL_NULL: {
			memcpy(result+length, "NULL", 4);
			length += 4;
			break;
		}
		
		case VAL_NUMBER: {
			long long int len = snprintf(NULL, 0, "%g", AS_NUMBER(b));
			char* str = ALLOCATE(char, len + 1);
			snprintf(str, len + 1, "%g", AS_NUMBER(b));
			memcpy(result + length, str, len);	
			length += len;
			break;		
			break;		
		}
		
		case VAL_OBJ: {
			if(!IS_STRING(b)) {
				runtimeError("\e[1;31mError: Invalid operands for string conversion. ");
			}
			
			ObjString* str = AS_STRING(b);
			memcpy(result + length, str->chars, str->length);
			length += str->length;
			break;
		}
		
		case VAL_NL: {
			memcpy(result + length, "\n", 1);
			length += 1;
			break;
		}
		
		default: {
			runtimeError("\e[1;31mError: Invalid operands for string conversion. ");
		}
			
	}
	
	switch(a.type) {
		case VAL_BOOL: {
			bool bl = AS_BOOL(a);
			if (bl == true) {
				memcpy(result+length, "true", 4);
				length += 4;
			} else {
				memcpy(result+length, "false", 5);
				length += 5;
			}
			break;
		}
		
		case VAL_NULL: {
			memcpy(result+length, "NULL", 4);
			length += 4;
			break;
		}
		
		case VAL_NUMBER: {
			long long int len = snprintf(NULL, 0, "%g", AS_NUMBER(a));
			char* str = ALLOCATE(char, len + 1);
			snprintf(str, len + 1, "%g", AS_NUMBER(a));
			memcpy(result + length, str, len);	
			length += len;
			break;		
		}
		
		case VAL_OBJ: {
			if(!IS_STRING(a)) {
				runtimeError("\e[1;31mError: Invalid operands for string conversion");
			}
			ObjString* str = AS_STRING(a);
			memcpy(result + length, str->chars, str->length);
			length += str->length;
			break;
		}
		
		case VAL_NL: {
			memcpy(result + length, "\n", 1);
			length += 1;
			break;
		}
		
		default: {
			runtimeError("\e[1;31mError: Invalid operands for string conversion.");
		}
			
	}
	

	result[length] = '\0';
	ObjString* output = takeString(result, length);
	pop(2);
	push(OBJ_VAL(output));	
}

static InterpretResult run(bool REPLmode) {
	CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants->values[READ_BYTE()])
#define READ_LONG_CONSTANT() (frame->closure->function->chunk.constants->values[frame->closure->function->chunk.code[(int)(frame->ip - frame->closure->function->chunk.code)+1] |\
			      frame->closure->function->chunk.code[(int)(frame->ip - frame->closure->function->chunk.code)+2] << 8 |\
			      frame->closure->function->chunk.code[(int)(frame->ip - frame->closure->function->chunk.code)+3] << 16])
#define READ_SHORT() \
	(frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op)\
	do { \
		if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {\
			runtimeError("\e[1;31mError: Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		}\
		double b = AS_NUMBER(pop(1)); \
		Value* stackTop = vm.stackTop - 1; \
		AS_NUMBER(*stackTop) = AS_NUMBER(*stackTop) op b; \
	} while (false)

#define MOD_OP(valueType, op)\
	do { \
		if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {\
			runtimeError("\e[1;31mError: Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		}\
		int b = AS_NUMBER(pop(1)); \
		Value* stackTop = vm.stackTop - 1; \
		AS_NUMBER(*stackTop) = ((int)AS_NUMBER(*stackTop)) op b; \
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
		disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
#endif
		Value* aPtr;
		Value b;
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
					runtimeError("\e[1;31mError: Undefined variable '%.*s', ", name->length, name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				push(value);
				break;
			}
			
			case OP_DEFINE_GLOBAL: {
				ObjString* name = READ_STRING();
				tableSet(&vm.globals, &OBJ_KEY(name), peek(0));
				pop(1);
				break;
			}
			
			case OP_SET_GLOBAL:{
				ObjString* name = READ_STRING();
				if (tableSet(&vm.globals, &OBJ_KEY(name), peek(0))) {
					tableDelete(&vm.globals, &OBJ_KEY(name));
					runtimeError("\e[1;31mError: Undefined variable '%.*s', ", name->length, name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			
			case OP_GET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				push(*frame->closure->upvalues[slot]->location);
				break;
			}
			
			case OP_SET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				*frame->closure->upvalues[slot]->location = peek(0);
				break;
			}
			
			case OP_GET_PROPERTY: {
				if (!IS_INSTANCE(peek(0))) {
					runtimeError("\e[1;31mError: Attempt to access property of a non-instance, ");
					return INTERPRET_RUNTIME_ERROR;
				}
				ObjInstance* instance = AS_INSTANCE(peek(0));
				ObjString* name = READ_STRING();
				
				Value value;
				if (tableGet(&instance->fields, &OBJ_KEY(name), &value)) {
					pop(1);
					push(value);
					break;
				}
				
				//runtimeError("Error: Undefined property '%.*s', ", name->length, name->chars);
				if (!bindMethod(instance->c, name)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			
			case OP_SET_PROPERTY: {
				if (!IS_INSTANCE(peek(1))) {
					runtimeError("\e[1;31mError: Only instances have fields, ");
					return INTERPRET_RUNTIME_ERROR;
				}
				ObjInstance* instance = AS_INSTANCE(peek(1));
				tableSet(&instance->fields, &OBJ_KEY(READ_STRING()), peek(0));
				
				Value value = pop(1);
				pop(1);
				push(value);
				break;
			}
			
			case OP_DELATTR: {
				ObjString* attr = AS_STRING(pop(1));
				ObjInstance* instance = AS_INSTANCE(pop(1));
				Value value;
				if (tableGet(&instance->fields, &OBJ_KEY(attr), &value)) {
					tableDelete(&instance->fields, &OBJ_KEY(attr));
					break;
				}
				
				runtimeError("\e[1;31mError: Attempt to delete non-existent field '%.*s', ", attr->length, attr->chars);
				break;
			}
			
			case OP_GET_BASE: {
				ObjString* name = READ_STRING();
				ObjClass* baseClass = AS_CLASS(pop(1));
				if (!bindMethod(baseClass, name)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			
			case OP_EQUAL: {
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesEqual(*aPtr, b));
				break;
			}
			
			case OP_SWITCH_EQUAL: {
				aPtr = vm.stackTop - 1;
				if (switchFallThrough) {
					*aPtr = BOOL_VAL(true);
					switchFallThrough = false;
				} else {
					*aPtr = BOOL_VAL(valuesEqual(*aPtr, *(aPtr - 1)));
				}
				break;
			}
			
			case OP_NOT_EQUAL: {
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesNotEqual(*aPtr, b));
				break;
			// Make these work for non-number types as well
			}
			
			case OP_GREATER: {
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesGreater(*aPtr, b));
				break;
			}
			
			case OP_GREATER_EQUAL: {
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesGreaterEqual(*aPtr, b));
				break;
			}
			
			case OP_LESS: {
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesLess(*aPtr, b));
				break;
			}
			
			case OP_LESS_EQUAL: {
				b = pop(1);
				aPtr = vm.stackTop - 1;
				*aPtr = BOOL_VAL(valuesLessEqual(*aPtr, b));
				break;
			}
			
			case OP_TERNARY: {
				b = pop(1);
				Value a = pop(1);
				Value* conditional = vm.stackTop - 1;
				*conditional = AS_BOOL(*conditional) ? a : b;
				break;
			}
			
			case OP_ADD: {
				if(IS_STRING(peek(0)) && IS_STRING(peek(1))) {
					concatenate();
				} else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
					double b = AS_NUMBER(pop(1));
					Value* stackTop = vm.stackTop - 1; 
		AS_NUMBER(*stackTop) = AS_NUMBER(*stackTop)+ b;
				} else if (IS_STRING(peek(0)) || IS_STRING(peek(1)) || IS_NL(peek(0)) || IS_NL(peek(0))) {
					convconcatenate();
				} else {
					runtimeError("\e[1;31mError: Operands must be two numbers or two strings, ");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
			
			case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
			
			case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
			
			case OP_MOD: MOD_OP(NUMBER_VAL, %); break;
			case OP_NOT:
				push(BOOL_VAL(isFalsey(pop(1))));
				break;
			
			case OP_NEGATE: {
				if(!IS_NUMBER(peek(0))) {
					runtimeError("\e[1;31mError: Operand must be a number, ");
					return INTERPRET_RUNTIME_ERROR;
				}
				
				// Check here for errors
				Value* valueToNegate = vm.stackTop - 1;
				
				(AS_NUMBER(*valueToNegate)) = 0 - (AS_NUMBER(*valueToNegate)); break;
			}
			
			case OP_PRINT: {
				printValue(pop(1));
				if (REPLmode) printf("\n");
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
				frame->ip += offset;
				break;
			}
			
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
			
			case OP_INVOKE: {
				ObjString* method = READ_STRING();
				int argCount = READ_BYTE();
				if (!invoke(method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}
			
			case OP_BASE_INVOKE: {
				ObjString* method = READ_STRING();
				int argCount = READ_BYTE();
				ObjClass* baseClass = AS_CLASS(pop(1));
				if (!invokeFromClass(baseClass, method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}
			
			case OP_CLOSURE: {
				ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
				ObjClosure* closure = newClosure(function);
				push(OBJ_VAL(closure));
				
				for (int i = 0; i < closure->upvalueCount; i++) {
					uint8_t isLocal = READ_BYTE();
					uint8_t index = READ_BYTE();
					if(isLocal) {
						closure->upvalues[i] = captureUpValue(frame->slots + index);
					} else {
						closure->upvalues[i] = frame->closure->upvalues[index];
					}
				}
				
				break;
			}
			
			case OP_CLOSE_UPVALUE: {
				closeUpvalues(vm.stackTop - 1);
				pop(1);
				break;
			}
			
			case OP_RETURN: {
				Value result = pop(1);
				
				closeUpvalues(frame->slots);
				
				vm.frameCount--;
				if (vm.frameCount == 0) {
					pop(1);
					return INTERPRET_OK;
				}
				
				int temp = vm.stackTop - frame->slots;
				vm.stackTop = frame->slots;
				push(result);
				vm.stack.count -= temp;
				
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}
			
			case OP_CLASS: {
				push(OBJ_VAL(newClass(READ_STRING())));
				break;
			}
			
			case OP_INHERIT: {
				Value baseClass = peek(1);
				if (!IS_CLASS(baseClass)) {
					runtimeError("\e[1;31mError: Attempt to inherit from non-class object.");
					return INTERPRET_RUNTIME_ERROR;
				}
				ObjClass* derivedClass = AS_CLASS(peek(0));
				tableAddAll(&AS_CLASS(baseClass)->methods, &derivedClass->methods);
				pop(1); // pop the derived class
				break;
			}
			
			case OP_METHOD: {
				defineMethod(READ_STRING());
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

InterpretResult interpret(const char* source, size_t len, bool REPLmode, bool* withinREPL) {
	if (!REPLmode) {
		// Not REPL mode
		strlen(source);
		ObjFunction* function = compile(source, len, REPLmode, *withinREPL);
		if (function == NULL) {
			clearLineInfo();
			return INTERPRET_COMPILE_ERROR;
		}
	
		push(OBJ_VAL(function));
		ObjClosure* closure = newClosure(function);
		pop(1);
		push(OBJ_VAL(closure));
		callValue(OBJ_VAL(closure), 0);
	
		InterpretResult result = run(REPLmode);
		clearLineInfo();
		return result;
	} else {
		// REPL mode
		ObjFunction* function = compileREPL(source, len, REPLmode, *withinREPL);
		if (function == NULL) {
			*withinREPL = true;
			clearLineInfo();
			return INTERPRET_COMPILE_ERROR;
		}
	
		push(OBJ_VAL(function));
		ObjClosure* closure = newClosure(function);
		pop(1);
		push(OBJ_VAL(closure));
		callValue(OBJ_VAL(closure), 0);
	
		InterpretResult result = run(REPLmode);
		clearLineInfo();
		*withinREPL = true;
		return result;
	}
}

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
	(type*)allocateObject(sizeof(type), objectType)
	
static Obj* allocateObject(size_t size, ObjType type) {
	Obj* object = (Obj*)reallocate(NULL, 0, size);
	object->type = type;
	object->isMarked = false;
	
	object->next = vm.objects;
	vm.objects = object;
	return object;
}

ObjClosure* newClosure(ObjFunction* function) {
	ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
	
	for (int i = 0; i < function->upvalueCount; i++) {
		upvalues[i] = NULL;
	}
	
	ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

ObjFunction* newFunction(ValueArray* constants) {
	ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
	
	function->arity = 0;
	function->upvalueCount = 0;
	function->name = NULL;
	initChunk(&function->chunk, constants);
	return function;
}

ObjNative* newNative(NativeFunction function) {
	ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
	native->function = function;
	return native;
}

static uint32_t hashString(const char* key, int length) {
	uint32_t hash = 2166136261u;
	
	for (int i = 0; i < length; i++) {
		hash ^= key[i];
		hash *= 16777619;
	}
}

// does not own their copy of character array. Points back to source string but doesn't free source string
ObjString* allocateString(bool ownString, const char* chars, int length) {
	// Check whether string is stored in string table already
	uint32_t hash = hashString(chars, length);
	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) { //  if string is stored already in string table
		if (ownString) {
			FREE_ARRAY(char, (char*)chars, length + 1);
		}
		return interned;
	}

	// ObjString points directly into the source code.
	ObjString* string = (ObjString*)allocateObject(sizeof(ObjString), OBJ_STRING);
	string->length = length;
	string->chars = chars;
	string->ownString = ownString;
	string->hash = hash;
	
	push(OBJ_VAL(string));
	tableSet(&vm.strings, &OBJ_KEY(string), NULL_VAL);
	pop(1);
	
	return string;
}

ObjUpvalue* newUpvalue(Value* slot) {
	ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
	upvalue->closed = NULL_VAL;
	upvalue->location = slot;
	upvalue->next = NULL;
	return upvalue;
}

// own their copy of the character array. Points back to heap allocated string and frees string.
ObjString* takeString(const char* chars, int length) {
	return allocateString(true, chars, length);
}

static void printFunction(ObjFunction* function) {
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	
	printf("<def %.*s>", function->name->length, function->name->chars);
}

void printObject(Value value) {
	switch(OBJ_TYPE(value)) {
		case OBJ_CLOSURE:
			printFunction(AS_CLOSURE(value)->function);
			break;
		case OBJ_FUNCTION: {
			printFunction(AS_FUNCTION(value));
			break;
		}
		
		case OBJ_NATIVE: {
			printf("<native function>");
			break;
		}
		
		case OBJ_STRING: {
			printf("%.*s", AS_STRING(value)->length, AS_CSTRING(value));
			break;
		}
		
		case OBJ_UPVALUE:
			printf("upvalue");
			break;
	}
}

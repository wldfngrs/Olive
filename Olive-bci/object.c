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
	
	object->next = vm.objects;
	vm.objects = object;
	return object;
}

// does not own their copy of character array. Points back to source string but doesn't free source string
ObjString* allocateString(const char* chars, int length) {
	// ObjString points directly into the source code.
	ObjString* string = (ObjString*)allocateObject(sizeof(ObjString), OBJ_STRING);
	string->length = length;
	string->chars = chars;
	string->ownString = false;
	return string;
}

// own their copy of the character array. Points back to heap allocated string and frees string.
ObjString* takeString(bool ownString, int length) {
	ObjString* string = (ObjString*)allocateObject(sizeof(ObjString), OBJ_STRING);
	string->length = length;
	string->ownString = true;
	return string;
}

void printObject(Value value) {
	switch(OBJ_TYPE(value)) {
		case OBJ_STRING:
			printf("%s", AS_CSTRING(value));
			break;
	}
}

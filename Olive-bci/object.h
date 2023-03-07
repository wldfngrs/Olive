#ifndef olive_object_h
#define olive_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "table.h"

#define OBJ_TYPE(value)		(AS_OBJ(value)->type)

#define IS_FUNCTION(value)	isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)	isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)	isObjType(value, OBJ_STRING)

#define AS_FUNCTION(value)	((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)	(((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)	((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)	(((ObjString*)AS_OBJ(value))->chars)

typedef enum {
	OBJ_FUNCTION,
	OBJ_NATIVE,
	OBJ_STRING,
} ObjType;

struct Obj {
	ObjType type;
	struct Obj* next;
};

typedef struct {
	Obj obj;
	int arity;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

typedef Value (*NativeFunction)(int argCount, Value* args);

typedef struct {
	Obj obj;
	NativeFunction function;
} ObjNative;

struct ObjString {
	Obj obj;
	int length;
	bool ownString;
	const char* chars;
	uint32_t hash;	
};

ObjFunction* newFunction(ValueArray* constants);
ObjNative* newNative(NativeFunction function);
ObjString* takeString(const char* chars, int length);
ObjString* allocateString(bool ownString, const char* chars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type){
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif

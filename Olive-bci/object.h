#ifndef olive_object_h
#define olive_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "table.h"

#define OBJ_TYPE(value)		(AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value)	isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)		isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)	isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)	isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)	isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)	isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)	isObjType(value, OBJ_STRING)

#define AS_BOUND_METHOD(value)	((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)		((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)	((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)	((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)	((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value)	(((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)	((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)	(((ObjString*)AS_OBJ(value))->chars)

typedef enum {
	OBJ_BOUND_METHOD,
	OBJ_CLASS,
	OBJ_CLOSURE,
	OBJ_FUNCTION,
	OBJ_INSTANCE,
	OBJ_NATIVE,
	OBJ_STRING,
	OBJ_UPVALUE,
} ObjType;

struct Obj {
	ObjType type;
	bool isMarked;
	struct Obj* next;
};

typedef struct {
	Obj obj;
	int arity;
	int upvalueCount;
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

typedef struct ObjUpvalue {
	Obj obj;
	Value* location;
	Value closed;
	struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
	Obj obj;
	ObjFunction* function;
	ObjUpvalue** upvalues;
	int upvalueCount;
} ObjClosure;

typedef struct {
	Obj obj;
	ObjString* name;
	Value initCall;
	Table methods;
} ObjClass;

typedef struct {
	Obj obj;
	ObjClass* c;
	Table fields;
} ObjInstance;

typedef struct {
	Obj obj;
	Value reciever;
	ObjClosure* method;
} ObjBoundMethod;

ObjBoundMethod* newBoundMethod(Value reciever, ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction(ValueArray* constants);
ObjInstance* newInstance(ObjClass* c);
ObjNative* newNative(NativeFunction function);
ObjString* takeString(const char* chars, int length);
ObjString* allocateString(bool ownString, const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type){
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif

#ifndef olive_object_h
#define olive_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)		(AS_OBJ(value)->type)

#define IS_STRING(value)	isObjType(value, OBJ_STRING)

#define AS_STRING(value)	((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)	(((ObjString*)AS_OBJ(value))->chars)

typedef enum {
	OBJ_STRING,
} ObjType;

struct Obj {
	ObjType type;
	struct Obj* next;
};

struct ObjString {
	Obj obj;
	int length;
	bool ownString;
	const char* chars;	
};

ObjString* takeString(bool ownString, int length);
ObjString* allocateString(const char* chars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type){
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif

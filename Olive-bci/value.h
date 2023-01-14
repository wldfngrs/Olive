#ifndef olive_value_h
#define olive_value_h

#include "common.h"

typedef enum {
	VAL_BOOL,
	VAL_NULL,
	VAL_NUMBER,
} ValueType;

typedef struct {
	ValueType type;
	union {
		bool boolean;
		double number;
	} as;
} Value;

#define IS_BOOL(value)		((value).type == VAL_BOOL)
#define IS_NULL(value)		((value).type == VAL_NULL)
#define IS_NUMBER(value)	((value).type == VAL_NUMBER)

#define AS_BOOL(value)		((value).as.boolean)
#define AS_NUMBER(value)	((value).as.number)

#define BOOL_VAL(value)		((Value){VAL_BOOL, {.boolean = value}})
#define NULL_VAL         	((Value){VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value)	((Value){VAL_NUMBER, {.number = value}})

typedef struct {
	int capacity;
	int count;
	Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
bool valuesNotEqual(Value a, Value b);
bool valuesGreater(Value a, Value b);
bool valuesGreaterEqual(Value a, Value b);
bool valuesLess(Value a, Value b);
bool valuesLessEqual(Value a, Value b);
Value valuesConditional(Value a, Value b, Value conditional);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif

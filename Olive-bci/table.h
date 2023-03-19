#ifndef olive_table_h
#define olive_table_h

#include "common.h"
#include "value.h"

#define NULL_KEY         	((Key){VAL_NULL, {.number = 0}})
#define OBJ_KEY(object)		((Key){VAL_OBJ, {.obj = object}})

typedef struct {
	ValueType type;
	union {
		bool boolean;
		double number;
		ObjString* obj;
	} as;
	bool constant;
} Key;

typedef struct {
	Key key;
	Value value;
} Entry;

typedef struct {
	int count;
	int capacity;
	Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, Key* key, Value* value);
bool tableSet(Table* table, Key* key, Value value);
bool tableSetGlobal(Table* table, Key* key, Value value);
bool tableDelete(Table* table, Key* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void tableRemoveWhite(Table* table);
void markTable(Table* table);

#endif

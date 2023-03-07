#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

#define KEYASVALUE(key) ((Value*)key)

void initTable(Table* table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void freeTable(Table* table) {
	FREE_ARRAY(Entry, table->entries, table->capacity);
	initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, Key* key) {
	uint32_t index;
	
	Value* keyAsValue = KEYASVALUE(key);
	switch(keyAsValue->type) {
		case VAL_BOOL:
			index = (AS_BOOL(*keyAsValue) == true ? 1 : 0) % capacity;
			break;
		case VAL_NULL:
			index = 0;
			break;
		case VAL_NUMBER:
			index = (int)AS_NUMBER(*keyAsValue) % capacity;
			break;
		case VAL_OBJ:
			index = AS_STRING(*keyAsValue)->hash % capacity;
			break;
	}
	
	Entry* tombstone = NULL;
	
	for (;;) {
		Entry* entry = &entries[index];
		
		if (IS_NULL(entry->key)) {
			if(IS_NULL(entry->value)) {
				// Found an empty entry.
				return tombstone != NULL ? tombstone : entry;
			} else {
				// Found a tombstone.
				if (tombstone == NULL) tombstone = entry;
			}
		} else {
				// Found a non-empty, non-tombstone entry.
				switch((entry->key).type) {
					case VAL_BOOL:
						if (AS_BOOL(*keyAsValue) == AS_BOOL(entry->key)) return entry;
					case VAL_NUMBER:
						if (AS_NUMBER(*keyAsValue) == AS_NUMBER(entry->key)) return entry;
					case VAL_OBJ:
						if (AS_STRING(*keyAsValue) == AS_STRING(entry->key)) return entry;
						
				}
		}
		
		index = (index + 1) % capacity;
	}
}

bool tableGet(Table* table, Key* key, Value* value) {
	if (table->count == 0) return false;
	
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (IS_NULL(entry->key)) return false;
	
	*value = entry->value;
	return true;
}

static void adjustCapacity(Table* table, int capacity) {
	Entry* entries = ALLOCATE(Entry, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL_KEY;
		entries[i].value = NULL_VAL;
	}
	
	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		Entry* entry = &table->entries[i];
		if (IS_NULL(entry->key)) continue;
		
		Entry* dest = findEntry(entries, capacity, &entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}
	
	FREE_ARRAY(Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

bool tableSet(Table* table, Key* key, Value value) {
	if(table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capacity);
	}

	Entry* entry = findEntry(table->entries, table->capacity, key);
	
	bool isNewKey = IS_NULL(entry->key);
	if (isNewKey && IS_NULL(entry->value)) table->count++;
	
	entry->key = *key;
	entry->value = value;
	return isNewKey;
}

bool tableSetGlobal(Table* table, Key* key, Value value) {
	if(table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capacity);
	}

	Entry* entry = findEntry(table->entries, table->capacity, key);
	
	bool isNewKey = IS_NULL(entry->key);
	if (isNewKey && IS_NULL(entry->value)) table->count++;
	
	entry->key = *key;
	if (isNewKey) entry->value = value;
	return isNewKey;
}

bool tableDelete(Table* table, Key* key) {
	if(table->count == 0) return false;
	
	// Find the entry
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if(IS_NULL(entry->key)) return false;
	
	// Place a tombstone in the entry
	entry->key = NULL_KEY;
	entry->value = BOOL_VAL(true);
	
	return true;
}

// For method inheritance
void tableAddAll(Table* from, Table* to) {
	for (int i = 0; i < from->capacity; i++) {
		Entry* entry = &from->entries[i];
		
		if (IS_NULL(entry->key)) {
			tableSet(to, &entry->key, entry->value);
		}
	}
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
	if (table->count == 0) return NULL;
	
	uint32_t index = hash % table->capacity;
	
	for (;;) {
		Entry* entry = &table->entries[index];
		
		if (IS_NULL(entry->key)) {
			// Stop if we find an empty non-tombstone entry
			if(IS_NULL(entry->value)) return NULL;
		} else if (AS_STRING(entry->key)->length == length && AS_STRING(entry->key)->hash == hash && memcmp(AS_STRING(entry->key)->chars, chars, length) == 0) {
			return AS_STRING(entry->key);
		}
		index = (index + 1) % table->capacity;
	}
}

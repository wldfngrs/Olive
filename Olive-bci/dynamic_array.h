#include "memory.h"

struct dynamic_array {
	int count;
	int capacity;
	char* array;
};

void growDynamicArray(struct dynamic_array* da) {
	int oldCapacity = da->capacity;
	da->capacity = GROW_STACK_CAPACITY(oldCapacity); // large growth capacity is so string intern resolution happens infrequently.
	da->array = GROW_ARRAY(char, da->array, oldCapacity, da->capacity);
}

void appendDynamicArray(struct dynamic_array* da, char* in) {
	size_t len = strlen(in);
	if (da->count + len > da->capacity) {
		char* prev = da->array;
		growDynamicArray(da);
		char* cur = da->array;
		resolveStringInterns(cur - prev);
	}
	memcpy(da->array + da->count, in, len);
	da->count += len;
}

void freeDynamicArray(struct dynamic_array* da) {
	FREE_ARRAY(int, da->array, da->capacity);
	da->count = 0;
}

void initDynamicArray(struct dynamic_array* da) {
	da->count = 0;
	da->capacity = 0;
	da->array = NULL;
	growDynamicArray(da);
}


typedef struct dynamic_array DA;

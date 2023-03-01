#include "breakExit.h"
#include "memory.h"

void initBreakExit(breakExit* be) {
	be->count = 0;
	be->capacity = 0;
	be->exits = NULL;
	growBreakExit(be);
}

void growBreakExit(breakExit* be) {
	int oldCapacity = be->capacity;
	be->capacity = GROW_STACK_CAPACITY(oldCapacity);
	be->exits = GROW_ARRAY(int, be->exits, oldCapacity, be->capacity);
}

void freeBreakExit(breakExit* be) {
	FREE_ARRAY(int, be->exits, be->capacity);
	be->count = 0;
}

#include "control.h"
#include "memory.h"

void initControlFlow(controlFlow* control) {
	control->count = 0;
	control->capacity = 0;
	control->exits = NULL;
	control->continuePoint = 0;
	growControlFlow(control);
}

void growControlFlow(controlFlow* control) {
	int oldCapacity = control->capacity;
	control->capacity = GROW_STACK_CAPACITY(oldCapacity);
	control->exits = GROW_ARRAY(int, control->exits, oldCapacity, control->capacity);
}

void freeControlFlow(controlFlow* control) {
	FREE_ARRAY(int, control->exits, control->capacity);
	control->count = 0;
}

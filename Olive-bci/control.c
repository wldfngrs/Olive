#include "control.h"
#include "memory.h"

void initControlFlow(controlFlow* control) {
	control->count = 0;
	control->capacity = 0;
	control->exits = NULL;
	control->cpCount = 0;
	control->cpCapacity = 0;
	control->continuePoint = NULL;
	control->prev = NULL;
	growControlFlow(control);
	growCpControlFlow(control);
}

void growControlFlow(controlFlow* control) {
	int oldCapacity = control->capacity;
	control->capacity = GROW_STACK_CAPACITY(oldCapacity);
	control->exits = GROW_ARRAY(int, control->exits, oldCapacity, control->capacity);
}

void growCpControlFlow(controlFlow* control) {
	int oldCapacityCp = control->cpCapacity;
	control->cpCapacity = GROW_STACK_CAPACITY(oldCapacityCp);
	control->continuePoint = GROW_ARRAY(int, control->continuePoint, oldCapacityCp, control->cpCapacity);
}

void freeControlFlow(controlFlow* control) {
	FREE_ARRAY(int, control->exits, control->capacity);
	FREE_ARRAY(int, control->continuePoint, control->cpCapacity);
	control->count = 0;
	control->cpCount = 0;
}

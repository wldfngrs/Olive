#include "stack.h"
#include "memory.h"

void initStack(Stack* stack) {
	stack->count = 0;
	stack->capacity = 0;
	stack->stack = NULL;
	growStack(stack);
}

void growStack(Stack* stack) {
	int oldCapacity = stack->capacity;
	stack->capacity = GROW_STACK_CAPACITY(oldCapacity);
	stack->stack = GROW_ARRAY(Value, stack->stack, oldCapacity, stack->capacity);
}

void freeStack(Stack* stack) {
	FREE_ARRAY(Value, stack->stack, stack->capacity);
}

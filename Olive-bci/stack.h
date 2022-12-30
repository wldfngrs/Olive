#ifndef stack_olive_h
#define stack_olive_h

#include "value.h"

typedef struct {
	int count;
	int capacity;
	Value* stack;	
} Stack;

void initStack();
void freeStack();
void growStack(Stack* stack);

#endif

#ifndef stack_olive_h
#define stack_olive_h

#include "value.h"

typedef struct {
	int count;
	int capacity;
	Value* stack;	
} Stack;

void initStack(Stack* stack);
void freeStack(Stack* stack);
void growStack(Stack* stack);

#endif

#ifndef break_exit_h
#define break_exit_h


struct controlFlow {
	struct controlFlow* prev;
	int count;
	int capacity;
	int cpCapacity;
	int cpCount;
	int* exits;
	int* continuePoint;	
};

typedef struct controlFlow controlFlow;

void initControlFlow(controlFlow* control);
void freeControlFlow(controlFlow* control);
void growControlFlow(controlFlow* control);
void growCpControlFlow(controlFlow* control);

#endif

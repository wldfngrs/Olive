#ifndef break_exit_h
#define break_exit_h


typedef struct {
	int count;
	int capacity;
	int* exits;
	int continuePoint;	
} controlFlow;

void initControlFlow(controlFlow* control);
void freeControlFlow(controlFlow* control);
void growControlFlow(controlFlow* control);

#endif

#ifndef break_exit_h
#define break_exit_h


typedef struct {
	int count;
	int capacity;
	int* exits;	
} breakExit;

void initBreakExit(breakExit* be);
void freeBreakExit(breakExit* be);
void growBreakExit(breakExit* be);

#endif

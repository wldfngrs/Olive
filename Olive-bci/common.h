#ifndef olive_common_h
#define olive_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION

//#define DEBUG_STRESS_GC
//#define DEBUG_LOG_GC

#define SCOPE_COUNT 1000 // Increase to 32 bits if too little over time.

extern bool REPLmode;
extern bool withinREPL;

#endif

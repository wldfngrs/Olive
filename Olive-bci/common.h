#ifndef olive_common_h
#define olive_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION

#define SCOPE_COUNT 100000 // Increase to 32 bits if too little over time.

extern bool REPLmode;
extern bool withinREPL;

#endif

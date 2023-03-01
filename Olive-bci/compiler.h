#ifndef olive_compiler_h
#define olive_compiler_h

#include "vm.h"
#include "object.h"

#define BREAK 	currentChunk()->code[currentChunk()->count - 5] == OP_BREAK

bool compile(const char* source, Chunk* chunk);

#endif

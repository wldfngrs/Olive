#ifndef olive_compiler_h
#define olive_compiler_h

#include "vm.h"
#include "object.h"

ObjFunction* compile(const char* source, size_t len, bool REPLmode, bool withinREPL);
ObjFunction* compileREPL(const char* source, size_t len, bool REPLmode, bool withinREPL);
void markCompilerRoots();

#endif

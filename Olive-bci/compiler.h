#ifndef olive_compiler_h
#define olive_compiler_h

#include "vm.h"
#include "object.h"

ObjFunction* compile(const char* source);

#endif

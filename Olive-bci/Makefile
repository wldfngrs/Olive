C_SOURCES = ${wildcard *.c} 
HEADERS = ${wildcard *.h}

olive: ${C_SOURCES} ${HEADERS}
	gcc -g -o olive main.c chunk.c memory.c debug.c value.c vm.c stack.c compiler.c scanner.c object.c table.c control.c

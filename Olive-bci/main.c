#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include <stdio.h>

char* welcome_text = {
"        888\n"
" .d88b. 888  .d888   888   888 .d88b.\n"
"888  888888    888   888   888d8P  Y8b\n"
"888  888888    888   888  .d8 88888888\n"
"888  888888    888   888 .d8  8b.\n"
" \"Y88P\"  \"Y88P\"\"Y88P\" \"Y8Y\"   \"Y8888P\"\n"
};

char* version_text = {"Olive Interpreter v0.0.1 (Mar 27 2023, 17:53:14) [GCC 11.2.0] on linux\nCopyright(C) 2023 wldfngrs, https://github.com/wldfngrs/Olive"};


bool REPLmode = false;
int currentLength = 0;
int prevLength = 0;

static bool quit(char* line) {
	if ((strcmp(line + prevLength, "exit\n") * strcmp(line + prevLength, "quit\n")) == 0) {
		return true;
	}
	
	return false;
}

// modify hardcoded line limit
static void repl() {
	REPLmode = true;
	printf("%s\n", welcome_text);
	printf("\e[1;32m%s\n\n\e[0m", version_text);
	char line[1024];
	for(;;) {
		printf("> ");
		
		if (!fgets(line + prevLength, sizeof(line), stdin)) {
			printf("\n");
			break;
		}
		
		currentLength = strlen(line);

		
		if (quit(line)) {
			withinREPL = false;
			printf("Exiting Olive.\n\n");
			return;
		}
		
		interpretREPL(line + prevLength);
		//interpret(line);
		prevLength = currentLength;
	}
}

static int checkExtension(const char* path) {
	char* extension = strstr(path, ".");
	if (extension == NULL) {
		return -1;
	}
	
	if (memcmp(extension, ".olv", 4) == 0) {
		return 0;
	} else {
		return -1;
	}
}

static char* readFile(const char* path) {
	if (checkExtension(path) == -1){
		fprintf(stderr, "\e[1;31mWrong file type. File must be a .olv file\n\e[0m");
		exit(70);
	}
	
	FILE* file = fopen(path, "rb");
	if (file == NULL){
		fprintf(stderr, "\e[1;31mFailed to open file \"%s\".\n\e[0m", path);
		exit(74);
	}
	
	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);
	
	char* buffer = (char*)malloc(fileSize + 1);
	if (buffer == NULL) {
		fprintf(stderr, "\e[1;31mNot enough memory to read \"%s\".\n\e[0m", path);
		exit(74);
	}
	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
	if(bytesRead < fileSize) {
		fprintf(stderr, "\e[1;31mCould not read file \"%s\".\n\e[0m", path);
		exit(74);
	}
	buffer[bytesRead] = '\0';
	
	fclose(file);
	return buffer;
}

static void runFile(const char* path) {
	REPLmode = false;
	char* source = readFile(path);
	InterpretResult result = interpret(source);
	free(source);
	
	if (result == INTERPRET_COMPILE_ERROR) exit(65);
	if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
	initVM();
	
	if (argc == 1) {
		repl();
	} else if (argc == 2) {
		runFile(argv[1]);
	} else {
		fprintf(stderr, "Usage: olive [path]\n");
		exit(64);
	}
	freeVM();
	return 0;
}

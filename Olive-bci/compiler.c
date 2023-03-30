#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"
#include "chunk.h"
#include "control.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

bool scannedPastNewLine = false;
int breakGlobal = 0; // 0 if break not executed.
ValueArray constants;

typedef struct {
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT,
	PREC_TERNARY,
	PREC_OR,
	PREC_AND,
	PREC_EQUALITY,
	PREC_COMPARISON,
	PREC_INTERPOLATION,
	PREC_TERM,
	PREC_FACTOR,
	PREC_UNARY,
	PREC_CALL,
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

typedef struct {
	Token name;
	int depth;
	bool isConst;
	bool isCaptured;
} Local;

typedef struct {
	uint8_t index;
	bool isLocal;
} Upvalue;

typedef enum {
	TYPE_FUNCTION,
	TYPE_INITIALIZER,
	TYPE_METHOD,
	TYPE_SCRIPT,
} FunctionType;

struct Compiler {
	struct Compiler* enclosing;
	ObjFunction* function;
	FunctionType type;

	Local locals[SCOPE_COUNT];
	int localCount;
	Upvalue upvalues[SCOPE_COUNT];
	int scopeDepth;
};

typedef struct ClassCompiler {
	struct ClassCompiler* enclosing;
	Token name;
	bool hasBaseClass;
} ClassCompiler;

typedef struct Compiler Compiler;

Parser parser;

ClassCompiler* currentClass = NULL;

Compiler* current = NULL; // page 394. modify for "principled engineers"

Chunk* compilingChunk;

static Chunk* currentChunk() {
	return &current->function->chunk;
}

char returnString[256];

static char* trim (const char* string, int trimLength) {
	memset(returnString, 0, 256);
	strncpy(returnString, string, trimLength);
	return returnString;
}

static void errorAt(Token* token, const char* message) {
	if (parser.panicMode) return;
	parser.panicMode = true;

	fprintf(stderr, "\e[1;31m[line %d] Error", token->line);
	
	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == TOKEN_ERROR) {
		// nothing
	} else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}
	
	fprintf(stderr, ": %s\n\e[0m", message);
	parser.hadError = true;
}

static void error(const char* message) {
	errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
	errorAt(&parser.current, message);
}

static void advance() {
	Token temp = parser.previous;
	parser.previous = parser.current;
	
	for (;;) {
		parser.current = scanToken();
		// scan past all possible new line tokens
		if (parser.current.type == TOKEN_NEWLINE) {
			temp.type = TOKEN_NEWLINE;
			continue;
		}
		else if(parser.current.type != TOKEN_ERROR) {
			scannedPastNewLine = false;
			break;
		}
		
		errorAtCurrent(parser.current.start);
	}
	
	if (temp.type == TOKEN_NEWLINE) {
		scannedPastNewLine = true;
	}
}

static void consume(TokenType type, const char* message) {
	if (parser.current.type == type) {
		advance();
		return;
	}
	
	/*if (type == TOKEN_SEMICOLON) {
		fprintf(stderr, "[line %d] Error at end: Expect ';' after variable declaration\n", parser.current.line - 1);
		parser.hadError = true;
		return;
	}*/
		
	errorAtCurrent(message);
}

/*static void consumeIfCount(TokenType type, int* count) {	
	if (parser.current.type == type) {
		advance();
		(*count)++;
	} else {
		return;
	}
}*/

static void consumeIf(TokenType type) {
	if (parser.current.type == type) {
		advance();
		return;
	}
}

static bool check(TokenType type) {
	return parser.current.type == type;
}

static bool match(TokenType type) {
	if (!check(type)) return false;
	advance();
	return true;
}

static void emitByte(uint8_t byte) {
	writeChunk(currentChunk(), byte, parser.previous.line);
}
/*
static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}*/

static void emitLoop(int loopStart) {
	emitByte(OP_LOOP);
	
	int offset = currentChunk()->count - loopStart + 2;
	if(offset > UINT16_MAX) error("Loop body too large.");
	
	emitByte((offset >> 8) & 0xff);
	emitByte(offset & 0xff);
}

static void emitContinue(int* continueStart, uint8_t instruction) {
	emitByte(instruction);
	emitByte(0xff);
	emitByte(0xff);
	*continueStart = currentChunk()->count - 2;
}

static int emitJump(uint8_t instruction) {
	emitByte(instruction);
	emitByte(0xff);
	emitByte(0xff);
	return currentChunk()->count - 2;
}

/*
static uint8_t makeConstant(Value value) {
	int constant = addConstant(currentChunk(), value);
	return (uint8_t)constant;
}*/

static void emitConstant(Value value) {
	writeConstant(currentChunk(), value, parser.previous.line);
}

static void patchJump(int offset) {
	// -2 to adjust for the bytecode for the jump offset itself.
	int jump = currentChunk()->count - offset - 2;
	
	if (jump > UINT16_MAX) {
		error("Too much jump offset.");
	}
	
	currentChunk()->code[offset] = (jump >> 8) & 0xff;
	currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler, FunctionType type, ValueArray* constants) {
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	compiler->function = newFunction(constants);
	current = compiler;
	
	if (type != TYPE_SCRIPT) {
		current->function->name = allocateString(false, parser.previous.start, parser.previous.length);
	}

	Local* local = &current->locals[current->localCount++];
	local->depth = 0;
	local->isCaptured = false;
	if (type != TYPE_FUNCTION) {
		local->name.start = "this";
		local->name.length = 4;
	} else {
		local->name.start = "";
		local->name.length = 0;
	}
	clearLineInfo();
}

static void emitOpAndConstant(uint8_t byte, int constant) {
	emitByte(byte);
	if (constant < 256) {
		writeChunk(currentChunk(), (uint8_t)constant, parser.previous.line);
	} else {
		writeChunk(currentChunk(), (uint8_t)(constant & 0xff), parser.previous.line);
		writeChunk(currentChunk(), (uint8_t)((constant >> 8) & 0xff), parser.previous.line);
		writeChunk(currentChunk(), (uint8_t)((constant >> 16) & 0xff), parser.previous.line);
	}
}

static void emitReturn() {
	if (current->type == TYPE_INITIALIZER) {
		emitOpAndConstant(OP_GET_LOCAL, 0);
	} else {
		emitByte(OP_NULL);
	}
	
	emitByte(OP_RETURN);
}


static ObjFunction* endCompiler() {
	emitReturn();
	ObjFunction* function = current->function;
	
#ifdef DEBUG_PRINT_CODE
	if(!parser.hadError) {
		disassembleChunk(currentChunk(), function->name != NULL ? trim(function->name->chars, function->name->length) : "<script>");
	}
#endif
	
	current = current->enclosing;
	clearLineInfo();
	return function;
}

static void beginScope() {
	current->scopeDepth++;
}

static void endScope() {
	current->scopeDepth--;
	int popCount = 0;
	
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		if (current->locals[current->localCount - 1].isCaptured) {
			emitByte(OP_CLOSE_UPVALUE);
		} else {
			popCount++;
		}
		current->localCount--;
		
	}
	//emitByte(OP_POPN);
	//emitByte((uint8_t)popCount);
	emitOpAndConstant(OP_POPN, popCount);
}

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

/* If the key is a new key, the value for the key on the table is set. If not, it just returns a boolean to signify a new key or not. (true == new key, false == old key)
	For an old key, the the constness of 'value' has to be equal to the value returned at that index in the chunk's ValueArray. eg. if 'value' is 1 and it has a constness of 'true', the value returned at index 1 in the ValueArray has to be of constness 'true' else error.
	*/

static int identifierConstantDeclaration(Token* name, bool isConst) {
	ObjString* objString = allocateString(false, name->start, name->length);

	if (tableSetGlobal(&vm.globalConstantIndex, &OBJ_KEY(objString), NUMBER_VAL(currentChunk()->constants->count))) {
		return addConstant(currentChunk(), OBJ_VAL(objString), isConst);	
	} else {
		Value constantIndex;
		tableGet(&vm.globalConstantIndex, &OBJ_KEY(objString), &constantIndex);
		if (currentChunk()->constants->values[(int)AS_NUMBER(constantIndex)].isConst == true) {
			error("Attempt to re-declare identifier already declared with type qualifier 'const'.");
		}
		else if (currentChunk()->constants->values[(int)AS_NUMBER(constantIndex)].isConst == false
		&&
		!REPLmode) {
			error("Attempt to re-declare variable type qualifier.");
		}
		return (int)AS_NUMBER(constantIndex);
	}
}

static int identifierConstantSetGet(Token* name) {
	ObjString* objString = allocateString(false, name->start, name->length);
	Value constantIndex;
	if (!tableGet(&vm.globalConstantIndex, &OBJ_KEY(objString), &constantIndex)) {
		error("Attempt to access undeclared variable.");
	}
	
	return (int)AS_NUMBER(constantIndex);
}

static bool identifiersEqual(Token* a, Token* b) {
	if (a->length != b->length) return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
	for (int i = compiler->localCount - 1; i >= 0; i--) {
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {
				error("Attempt to read local variable in its own initializer.");
			}
			return i;
		}
	}
	
	return -1; // couldn't resolve name to any declared local variables
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
	int upvalueCount = compiler->function->upvalueCount;
	
	for (int i = 0; i < upvalueCount; i++) {
		Upvalue* upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->isLocal == isLocal) {
			return i;
		}
	}
	
	if (upvalueCount == SCOPE_COUNT) {
		error("Too many closure variables in function.");
		return 0;
	}
	
	compiler->upvalues[upvalueCount].isLocal = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
	if (compiler->enclosing == NULL) return -1;
	
	int local = resolveLocal(compiler->enclosing, name);
	if (local != -1) { // if resolved in enclosing compilers local variables array
		compiler->enclosing->locals[local].isCaptured = true;
		return addUpvalue(compiler, (uint8_t)local, true);
	}
	
	int upvalue = resolveUpvalue(compiler->enclosing, name);
	if (upvalue != -1) {
		return addUpvalue(compiler, (uint8_t)upvalue, false);
	}
	
	return -1;
}

static void addLocal(Token name, bool isConst) {
	if (current->localCount == SCOPE_COUNT) {
		error("Too many local variables in function.");
		return;
	}
	
	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1; // -1 depth means variable is declared but uninitialized
	local->isConst = isConst;
	local->isCaptured = false;
}

static void declareVariable(bool isConst) {
	// Global variables are implicitly declared.
	if (current->scopeDepth == 0) return;
	
	Token* name = &parser.previous;
	
	for (int i = current->localCount - 1; i >= 0; i--) {
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}
		
		if (identifiersEqual(name, &local->name)) {
			if (local->isConst != isConst) {
				error("Attempt to re-declare variable type qualifier.");
			}
			error("Variable re-definition within scope.");
		}
	}
	
	
	addLocal(*name, isConst);
}

static int parseVariable(const char* errorMessage, bool isConst) {
	consume(TOKEN_IDENTIFIER, errorMessage);
	
	declareVariable(isConst);
	if (current->scopeDepth > 0) return 0;
	
	return identifierConstantDeclaration(&parser.previous, isConst);
}

static void markInitialized() {
	if (current->scopeDepth == 0) return;
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(int global) {
	if (current->scopeDepth > 0) {
		markInitialized();
		return;
	}
	
	emitOpAndConstant(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList() {
	uint8_t argCount = 0;
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			
			// TODO: increase count
			if (argCount == 255) {
				error("Can't have more than 255 arguments.");
			}
			argCount++;
		} while (match(TOKEN_COMMA));
	}
	
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
	return argCount;
}

static void and_(bool canAssign) {
	int endJump = emitJump(OP_JUMP_IF_FALSE);
	
	emitByte(OP_POP);
	parsePrecedence(PREC_AND);
	
	patchJump(endJump);
}

static void ternary(bool canAssign) {
	// Parse then branch
	parsePrecedence(PREC_TERNARY);
	
	consume(TOKEN_COLON, "Expect ':' after then branch of ternary operator.");
	
	// Parse else branch
	parsePrecedence(PREC_ASSIGNMENT);
	emitByte(OP_TERNARY);	
}

static void binary(bool canAssign) {
	TokenType operatorType = parser.previous.type;
	
	ParseRule* rule = getRule(operatorType);

	parsePrecedence((Precedence)(rule->precedence + 1));
	
	switch(operatorType) {
		case TOKEN_BANG_EQUAL: emitByte(OP_NOT_EQUAL);
		case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
		case TOKEN_GREATER: emitByte(OP_GREATER); break;
		case TOKEN_GREATER_EQUAL: emitByte(OP_GREATER_EQUAL);
		case TOKEN_LESS: emitByte(OP_LESS); break;
		case TOKEN_LESS_EQUAL: emitByte(OP_LESS_EQUAL); break;
		case TOKEN_PLUS: emitByte(OP_ADD); break;
		case TOKEN_CONCAT: emitByte(OP_ADD); break;
		case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
		case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
		default:
			return;
	}
}

static void call(bool canAssign) {
	uint8_t argCount = argumentList();
	emitOpAndConstant(OP_CALL, argCount);
}

static void literal(bool canAssign) {
	switch(parser.previous.type) {
		case TOKEN_FALSE: emitByte(OP_FALSE); break;
		case TOKEN_NULL: emitByte(OP_NULL); break;
		case TOKEN_TRUE: emitByte(OP_TRUE); break;
		default:
			return;
	}
}

static void dot(bool canAssign) {
	consume(TOKEN_IDENTIFIER, "Expect property name after '.'");
	int name = addConstant(currentChunk(), OBJ_VAL(allocateString(false, parser.previous.start, parser.previous.length)), false);
	
	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitOpAndConstant(OP_SET_PROPERTY, name);
	} else if (match(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList();
		emitOpAndConstant(OP_INVOKE, name);
		emitByte(argCount);
	} else {
		emitOpAndConstant(OP_GET_PROPERTY, name);
	}
}


static void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void or_(bool canAssign) {
	int elseJump = emitJump(OP_JUMP_IF_FALSE);
	int endJump = emitJump(OP_JUMP);
	
	patchJump(elseJump);
	emitByte(OP_POP);
	
	parsePrecedence(PREC_OR);
	patchJump(endJump);
}

static void string(bool canAssign) {
	if (*(parser.previous.start) == '"') {
		emitConstant(OBJ_VAL(allocateString(false, parser.previous.start + 1, parser.previous.length - 2)));
	} else if (*(parser.previous.start) == ' ') {
		emitConstant(OBJ_VAL(allocateString(false, parser.previous.start, parser.previous.length - 1)));
	}
}

static void namedVariable(Token name, bool canAssign) {
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);
	bool global;
	if (arg != -1) {
		global = false;
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else if ((arg = resolveUpvalue(current, &name)) != -1) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	} else {
		global = true;
		arg = identifierConstantSetGet(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}
	
	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		if (currentChunk()->constants->values[arg].isConst == true && global) {
			error("Attempt to re-assign variable declared with type qualifier 'const'.");
		} else if (current->locals[arg].isConst == true && !global) {
			error("Attempt to re-assign variable declared with type qualifier 'const'.");
		} else emitOpAndConstant(setOp, (uint8_t)arg);
	} else {
		emitOpAndConstant(getOp, (uint8_t)arg);
	}
}

static void variable(bool canAssign) {
	namedVariable(parser.previous, canAssign);
}

static void interpolation(bool canAssign) {
	emitConstant(OBJ_VAL(allocateString(false, parser.previous.start, parser.previous.length - 1)));
}


static Token syntheticToken(const char* text) {
	Token token;
	token.start = text;
	token.length = (int)strlen(text);
	return token;
}

static void base_(bool canAssign) {
	if (currentClass == NULL) {
		error("Attempt to use 'base' token outside of a class");
	} else if (!currentClass->hasBaseClass) {
		error("Attempt to use 'base' token in a non-derived class.");
	}
	
	consume(TOKEN_DOT, "Expect '.' after 'base' token.");
	consume(TOKEN_IDENTIFIER, "Expect 'base class' method name.");
	uint8_t name = identifierConstantDeclaration(&parser.previous, true);
	
	namedVariable(syntheticToken("this"), false);
	if (match(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList();
		namedVariable(syntheticToken("base"), false);
		emitOpAndConstant(OP_BASE_INVOKE, name);
		emitByte(argCount);
	} else {
		namedVariable(syntheticToken("base"), false);
		emitOpAndConstant(OP_GET_BASE, name);
	}
}

static void this_(bool canAssign) {
	if (currentClass == NULL) {
		error("Attempt to use 'this' token outside of a class scope.");
		return;
	}
	
	variable(false);
}

static void unary(bool canAssign) {
	TokenType operatorType = parser.previous.type;
	
	parsePrecedence(PREC_UNARY);
	
	switch(operatorType) {
		case TOKEN_BANG: emitByte(OP_NOT); break;
		case TOKEN_MINUS: emitByte(OP_NEGATE); break;
		default:
			return;
	}
}

static void caseError(bool canAssign) {
	error("'case' token not within a 'switch' statement.");
}

static void defaultError(bool canAssign) {
	error("'default' token not within a 'switch' statement.");
}

static void breakError(bool canAssign) {
	error("'break' token not within loop or switch statement.");
}

static void continueError(bool canAssign) {
	error("'continue' token not within loop statement.");
}

static void parenError(bool canAssign) {
	error("Statement expected before ')' token.");
}

static void braceError(bool canAssign) {
	error("'}' without corresponding '{' token.");
}

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN]= {grouping,call,PREC_CALL},
	[TOKEN_RIGHT_PAREN]= {parenError,NULL,PREC_NONE},
	[TOKEN_LEFT_BRACE]= {NULL,NULL,PREC_NONE},
	[TOKEN_RIGHT_BRACE]= {braceError,NULL,PREC_NONE},
	[TOKEN_COMMA]= {NULL,NULL,PREC_NONE},
	[TOKEN_DOT]= {NULL,dot,PREC_CALL},
	[TOKEN_MINUS]= {unary,binary,PREC_TERM},
	[TOKEN_PLUS]= {NULL,binary,PREC_TERM},
	[TOKEN_SEMICOLON]= {NULL,NULL,PREC_NONE},
	[TOKEN_SLASH]= {NULL,binary,PREC_FACTOR},
	[TOKEN_STAR]= {NULL,binary,PREC_FACTOR},
	[TOKEN_QUESTION_MARK]= {NULL, ternary, PREC_TERNARY},
	[TOKEN_BANG]= {unary,NULL,PREC_NONE},
	[TOKEN_BANG_EQUAL]= {NULL,binary,PREC_EQUALITY},
	[TOKEN_EQUAL]= {NULL,NULL,PREC_NONE},
	[TOKEN_EQUAL_EQUAL]= {NULL,binary,PREC_EQUALITY},
	[TOKEN_GREATER]= {NULL,binary,PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL] = {NULL,binary,PREC_COMPARISON},
	[TOKEN_LESS]= {NULL,binary,PREC_COMPARISON},
	[TOKEN_LESS_EQUAL]= {NULL,binary,PREC_COMPARISON},
	[TOKEN_IDENTIFIER]= {variable,NULL,PREC_NONE},
	[TOKEN_STRING]= {string,NULL,PREC_NONE},
	[TOKEN_NUMBER]= {number,NULL,PREC_NONE},
	[TOKEN_AND]= {NULL,and_,PREC_NONE},
	[TOKEN_CLASS]= {NULL,NULL,PREC_NONE},
	[TOKEN_ELSE]= {NULL,NULL,PREC_NONE},
	[TOKEN_FALSE] = {literal, NULL, PREC_NONE},
	[TOKEN_FOR]= {NULL,NULL,PREC_NONE},
	[TOKEN_DEF]= {NULL,NULL,PREC_NONE},
	[TOKEN_IF]= {NULL,NULL,PREC_NONE},
	[TOKEN_NULL]= {literal,NULL,PREC_NONE},
	[TOKEN_OR]= {NULL,or_,PREC_NONE},
	[TOKEN_PRINT]= {NULL,NULL,PREC_NONE},
	[TOKEN_RETURN]= {NULL,NULL,PREC_NONE},
	[TOKEN_BASE]= {base_,NULL,PREC_NONE},
	[TOKEN_THIS]= {this_,NULL,PREC_NONE},
	[TOKEN_TRUE]= {literal,NULL,PREC_NONE},
	[TOKEN_VAR]= {NULL,NULL,PREC_NONE},
	[TOKEN_WHILE]= {NULL,NULL,PREC_NONE},
	[TOKEN_SWITCH] = {NULL,NULL,PREC_NONE},
	[TOKEN_SWITCHCASE] = {caseError,NULL,PREC_NONE},
	[TOKEN_SWITCHDEFAULT] = {defaultError, NULL, PREC_NONE},
	[TOKEN_BREAK] = {breakError, NULL, PREC_NONE},
	[TOKEN_CONTINUE] = {continueError, NULL, PREC_NONE},
	[TOKEN_CONST] = {NULL, NULL, PREC_NONE},
	[TOKEN_EOF]= {NULL,NULL,PREC_NONE},
	[TOKEN_INTERPOLATION] = {interpolation, NULL, PREC_NONE},
	[TOKEN_CONCAT] = {NULL, binary, PREC_INTERPOLATION}
};

static void parsePrecedence(Precedence precedence) {
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if(prefixRule == NULL) {
		error("Expect expression.");
		return;
	}
	
	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);
	
	while(precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}
	
	if (canAssign && match(TOKEN_EQUAL)) {
		error("Invalid assignment target.");
	}
}

static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

static void expression() {
	parsePrecedence(PREC_ASSIGNMENT);
}

static void continueParsingOnBreak1() {
	while (!scannedPastNewLine) {
		advance();
	}
}

static void continueParsingOnBreak2() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		advance();
	}
}

static void block(controlFlow* controls) {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration(controls);
		if (breakGlobal == 1) {
			continueParsingOnBreak2();
		}	
	}
	
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
	Compiler compiler;
	initCompiler(&compiler, type, &constants);
	beginScope();
	
	// compile the parameter list.
	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > 255) {
				errorAtCurrent("Too many parameters in function.");
			}
			
			uint8_t paramConstant = parseVariable("Expect parameter name.", false);
			defineVariable(paramConstant);
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
	
	// the body.
	consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block(NULL); // no control flow statements expected directly within function block.
	
	// create the function object.
	ObjFunction* function = endCompiler();
	emitOpAndConstant(OP_CLOSURE, addConstant(currentChunk(), OBJ_VAL(function), true));
	
	for (int i = 0; i < function->upvalueCount; i++) {
		emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
		emitByte(compiler.upvalues[i].index);
	}
}

static void method() {
	consume(TOKEN_IDENTIFIER, "Expect method name.");
	int constant = identifierConstantDeclaration(&parser.previous, true);
	
	FunctionType type = TYPE_METHOD;
	if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
		type = TYPE_INITIALIZER;
	}
	
	function(type);
	emitOpAndConstant(OP_METHOD, constant);
}

static void classDeclaration() {
	consume(TOKEN_IDENTIFIER, "Expect class name.");
	Token className = parser.previous;
	uint8_t nameConstant = identifierConstantDeclaration(&parser.previous, true);
	declareVariable(true);
	
	emitOpAndConstant(OP_CLASS, nameConstant);
	defineVariable(nameConstant);
	
	ClassCompiler classCompiler;
	classCompiler.name = parser.previous;
	classCompiler.hasBaseClass = false;
	classCompiler.enclosing = currentClass;
	currentClass = &classCompiler;
	
	if (match(TOKEN_COLON)) {
		consume(TOKEN_IDENTIFIER, "Expect 'base class' name.");
		variable(false);
		if (identifiersEqual(&className, &parser.previous)) {
			// Improve error message to include the actual class attempting to self-inherit.
			error("A class cannot inherit from itself.");
		}
		
		beginScope();
		addLocal(syntheticToken("base"), true);
		defineVariable(0);
		
		namedVariable(className, false);
		emitByte(OP_INHERIT);
		classCompiler.hasBaseClass = true;
	}
	
	namedVariable(className, false);
	consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
	while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		method();
	}
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
	emitByte(OP_POP);
	
	if (classCompiler.hasBaseClass) {
		endScope();
	}
	
	currentClass = currentClass->enclosing;
}

static void functionDeclaration() {
	uint8_t global = parseVariable("Expect function name.", true);
	markInitialized();
	function(TYPE_FUNCTION);
	defineVariable(global);
}

static void deleteAttribute() {
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'del_attr' token.");
	uint8_t argCount = 0;
	do {
		expression();
		argCount++;
	} while (match(TOKEN_COMMA) && argCount < 2);
	
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
	consume(TOKEN_SEMICOLON, "Expect ';' after 'del_attr' function call.");
	emitByte(OP_DELATTR);
}

static void varDeclaration(bool isConst) {
	int global = parseVariable("Expect variable name.", isConst);
	
	if (match(TOKEN_EQUAL)) {
		expression();
	} else {
		emitByte(OP_NULL);
	}
	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
	
	defineVariable(global);
}

static void expressionStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(OP_POP);
}

static void breakStatement(controlFlow* controls) {
	if (controls->capacity < controls->count + 1) {
		growControlFlow(controls);
	}
	controls->exits[controls->count++] = emitJump(OP_BREAK);
	breakGlobal = 1;
	consume(TOKEN_SEMICOLON, "Expect ';' after 'break' statement.");
}

int switchLevel = 0;
static void continueStatement(controlFlow* controls) {
	if (controls->cpCapacity < controls->cpCount + 1) {
		growCpControlFlow(controls);
	}
	controls->continuePoint[controls->cpCount++] = emitJump(OP_CONTINUE);
	consume(TOKEN_SEMICOLON, "Expect ';' after 'continue' statement.");
}

int loopLevel = 0;
static void forStatement() {
	beginScope();
	
	int loopVariableSlot = -1;
	Token loopVariableName;
	loopVariableName.start = NULL;
	
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for' token.");
	if (match(TOKEN_SEMICOLON)) {
		// No initializer
	} else if (match(TOKEN_VAR)) {
		loopVariableName = parser.current;
		varDeclaration(false);
		loopVariableSlot = current->localCount - 1;
	} else {
		expressionStatement();
	}

	int loopStart = currentChunk()->count;
	int exitJump = -1;
	controlFlow controls;
	initControlFlow(&controls);
	
	if (!match(TOKEN_SEMICOLON)) {
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after loop condition");
		
		// Jump out of the loop if the condition is false
		exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP);  // pop the result of the condition statement/expression
	}
	
	if (!match(TOKEN_RIGHT_PAREN)) {
		int bodyJump = emitJump(OP_JUMP);
		
		int incrementStart = currentChunk()->count;
		expression();
		emitByte(OP_POP);
		consume(TOKEN_RIGHT_PAREN, "Expect ')' after 'for' clauses.");
		
		emitLoop(loopStart);
		loopStart = incrementStart;
		patchJump(bodyJump);
	}
	
	int innerVariable = -1;
	if (loopVariableSlot != -1) {
		beginScope();
		emitByte(OP_GET_LOCAL);
		emitByte((uint8_t)loopVariableSlot);
		addLocal(loopVariableName, false);
		markInitialized();
		
		innerVariable = current->localCount - 1;
	}
	
	loopLevel++;
	
	if (parser.current.type == TOKEN_LEFT_BRACE) {
		declaration(&controls);
	} else {
		while(!scannedPastNewLine) {
			declaration(&controls);
			if (breakGlobal == 1) {
				continueParsingOnBreak1();
			}
		}
	}
	
	for (int i = 0; i < controls.cpCount; i++) {
		patchJump(controls.continuePoint[i]);
	}
	
	if (loopVariableSlot != -1) {
		emitByte(OP_GET_LOCAL);
		emitByte((uint8_t)innerVariable);
		emitByte(OP_SET_LOCAL);
		emitByte((uint8_t)loopVariableSlot);
		emitByte(OP_POP);
		
		endScope();
	}
	
	if (breakGlobal == 1) breakGlobal = 0;

	emitLoop(loopStart);
	
	int jumpPop = emitJump(OP_JUMP);
	if (exitJump != -1) patchJump(exitJump);
	emitByte(OP_POP);
	patchJump(jumpPop);
	
	for (int i = 0; i < controls.count; i++) {
		patchJump(controls.exits[i]);
	}
	
	endScope();
	freeControlFlow(&controls);
	loopLevel--;
}


static void ifStatement(controlFlow* controls) {
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
	
	int thenJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	
	if (parser.current.type == TOKEN_LEFT_BRACE) {
			declaration(controls);
	} else {
		while(!scannedPastNewLine) {
			declaration(controls);
			if (breakGlobal == 1) {
				continueParsingOnBreak1();
			}
		}
	}
	
	if (breakGlobal == 1) breakGlobal = 0;
	
	int elseJump = emitJump(OP_JUMP);
	
	patchJump(thenJump);
	emitByte(OP_POP);
	
	if (match(TOKEN_ELSE)) {
		if (parser.current.type == TOKEN_LEFT_BRACE) 	{
			declaration(controls);
		} else {
			while(!scannedPastNewLine) {
				declaration(controls);
				if (breakGlobal == 1) {
					continueParsingOnBreak1();
				}
			}
		}	
		
	}
	
	if (breakGlobal == 1) breakGlobal = 0;
	
	patchJump(elseJump);
}

static void switchStatement(controlFlow* prevControl) {
	switchLevel++;
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch' token.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after 'switch' expression.");
	
	consume(TOKEN_LEFT_BRACE, "Expect '{' to open 'switch' statement");
	beginScope();

	int jumpPresentCase = 0;
	controlFlow controls;
	initControlFlow(&controls);
	controls.prev = prevControl;
	
	for(int i = 0;;i++) {
		if ((parser.current.type == TOKEN_RIGHT_BRACE) || (parser.current.type == TOKEN_SWITCHDEFAULT)) {
			break;
		}
		consume(TOKEN_SWITCHCASE, "Expect 'case' token.");
		expression();
		consume(TOKEN_COLON, "Expect ':' before case statement.");
		
		emitByte(OP_SWITCH_EQUAL);
	
		jumpPresentCase = emitJump(OP_JUMP_IF_FALSE);
		
		emitByte(OP_POP); // pop true off stack
		
		if (parser.current.type == TOKEN_LEFT_BRACE) {
			declaration(&controls);
		} else {
			while(!scannedPastNewLine) {
				declaration(&controls);
				if (breakGlobal == 1) {
					continueParsingOnBreak1();
				}
			}
		}
		
		if (breakGlobal == 1) {
			breakGlobal = 0;
		} else {
			emitByte(OP_FALLTHROUGH);
		}
		
		int jumpPop = emitJump(OP_JUMP);
		if (jumpPresentCase != 0) {
			patchJump(jumpPresentCase);
			emitByte(OP_POP); // pop false off stack
		}
		
		patchJump(jumpPop);
	}
	
	consumeIf(TOKEN_SWITCHDEFAULT);
	if (parser.previous.type == TOKEN_SWITCHDEFAULT) {
		//breakGlobal = 0;
		int exitDefault;
		consume(TOKEN_COLON, "Expect ':' after 'default' token");
		
		if (parser.current.type == TOKEN_LEFT_BRACE) {
			declaration(&controls);
		} else {
			while(!scannedPastNewLine) {
				declaration(&controls);
				if (breakGlobal == 1) {
					continueParsingOnBreak1();
				}
			}
		}
		
		if (breakGlobal == 1) {
			breakGlobal = 0;
		}
	}
	
	for (int i = 0; i < controls.count; i++) {
		patchJump(controls.exits[i]);
	}
	emitByte(OP_POP);
	
	consume(TOKEN_RIGHT_BRACE, "Expect '}' to close 'switch' statement.");
	endScope();
	freeControlFlow(&controls);
	switchLevel--;
}

static void printStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
}

static void returnStatement() {
	if (current->type == TYPE_SCRIPT) {
		error("'return' token not within a function statement.");
	}
	
	if (match(TOKEN_SEMICOLON)) {
		emitReturn();
	} else {
		if (current->type == TYPE_INITIALIZER) {
			error("Can't return a value from an initiliazer.");
		}
		
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
		emitByte(OP_RETURN);
	}
}

static void whileStatement() {
	int breakJump = 0;
	
	loopLevel++;
	int loopStart = currentChunk()->count;
	controlFlow controls;
	initControlFlow(&controls);
	//controls.continuePoint = loopStart;
	int exitIndex = 0;
	
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while' statement.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
	
	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	
	if (parser.current.type == TOKEN_LEFT_BRACE) {
		declaration(&controls);
	} else {
		while(!scannedPastNewLine) {
			declaration(&controls);
			if (breakGlobal == 1) {
				continueParsingOnBreak1();
			}
		}
	}
	
	if (breakGlobal == 1) breakGlobal = 0;
	
	for (int i = 0; i < controls.cpCount; i++) {
		patchJump(controls.continuePoint[i]);
	}
	
	emitLoop(loopStart);
	
	patchJump(exitJump);
	emitByte(OP_POP);
	
	for (int i = 0; i < controls.count; i++) {
		patchJump(controls.exits[i]);
	}
	
	freeControlFlow(&controls);
	loopLevel--;
}

static void synchronize() {
	parser.panicMode = false;
	
	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON) return;
		
		switch (parser.current.type) {
			case TOKEN_CLASS:
			case TOKEN_DEF:
			case TOKEN_VAR:
			case TOKEN_FOR:
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_PRINT:
			case TOKEN_RETURN:
				return;
			default:
				// do nothing lol.
				;
		}
		
		advance();
	}
}

// The base idea: loop and switch statements have any break statements defined and exclusive to them. If statements on the other hand, inherit the break statement of the switch or loop statement that they're parsed within.

static void declaration(controlFlow* controls) {
	if (match(TOKEN_CLASS)) {
		classDeclaration();
	} else if (match(TOKEN_DEF)){
		functionDeclaration();	
	} else if (match(TOKEN_VAR)) {
		varDeclaration(false);
	} else if (match(TOKEN_DELATTR)) {
		deleteAttribute();
	} else {
		statement(controls);
	}
	
	if (parser.panicMode) synchronize();
}

static void constDeclaration() {
	varDeclaration(true);
}

// The base idea: loop and switch statements have any break statements defined and exclusive to them. If statements on the other hand, inherit the break statement of the switch or loop statement that they're parsed within.

static void statement(controlFlow* controls) {
	if(match(TOKEN_PRINT)) {
		printStatement();
	} else if (match(TOKEN_BREAK)) {
		if (loopLevel == 0 && switchLevel == 0) breakError(false);
		breakStatement(controls);
	} else if (match(TOKEN_CONTINUE)) {
		if (loopLevel == 0) continueError(false);
		if (switchLevel > 0) {
			emitByte(OP_POP);
			continueStatement(controls->prev);
			return;
		}
		
		continueStatement(controls);
	} else if (match(TOKEN_FOR)) {
		forStatement();
	} else if (match(TOKEN_IF)) {
		ifStatement(controls);
	} else if (match(TOKEN_RETURN)) {
		returnStatement();
	} else if (match(TOKEN_SWITCH)) {
		switchStatement(controls);
	} else if (match(TOKEN_WHILE)) {
		whileStatement();
	} else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block(controls);
		endScope();
	} else if (match(TOKEN_CONST)) {
		constDeclaration();
	} else {
		expressionStatement();
	}
}

static void addNativeIdentifiers() {
	for (int i = 0; i < vm.nativeIdentifierCount; i++) {
		ObjString* key = allocateString(false, vm.nativeIdentifiers[i], strlen(vm.nativeIdentifiers[i]));
		int index = addConstant(currentChunk(), OBJ_VAL(key), true);
		
		tableSet(&vm.globalConstantIndex, &OBJ_KEY(key), NUMBER_VAL(index));
	}
}

ObjFunction* compile(const char* source) {
	initValueArray(&constants);
	
	initScanner(source);
	Compiler compiler;
	initCompiler(&compiler, TYPE_SCRIPT, &constants);
	addNativeIdentifiers();
	//controlFlow* controls;
	
	parser.hadError = false;
	parser.panicMode = false;
	advance();
	
	while (!match(TOKEN_EOF)) {
		declaration(NULL);
	}
	
	ObjFunction* function = endCompiler();
	return parser.hadError ? NULL : function;
}

ObjFunction* compileREPL(const char* source) {
	static ValueArray constants;
	if (!withinREPL) initValueArray(&constants);
	
	initScanner(source);
	Compiler compiler;
	initCompiler(&compiler, TYPE_SCRIPT, &constants);
	addNativeIdentifiers();
	//controlFlow* controls;
	
	parser.hadError = false;
	parser.panicMode = false;
	advance();
	
	while (!match(TOKEN_EOF)) {
		declaration(NULL);
	}
	
	ObjFunction* function = endCompiler();
	return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
	Compiler* compiler = current;
	while (compiler != NULL) {
		markObject((Obj*)compiler->function);
		compiler = compiler->enclosing;
	}
}

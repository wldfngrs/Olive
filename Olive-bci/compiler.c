#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "chunk.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

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
	bool constant;
} Local;

typedef struct {
	Local locals[SCOPE_COUNT];
	int localCount;
	int scopeDepth;
} Compiler;

Parser parser;

Compiler* current = NULL; // page 394. modify for "principled engineers"

Chunk* compilingChunk;

static Chunk* currentChunk() {
	return compilingChunk;
}

static void errorAt(Token* token, const char* message) {
	if (parser.panicMode) return;
	parser.panicMode = true;

	fprintf(stderr, "[line %d] Error", token->line);
	
	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == TOKEN_ERROR) {
		// nothing
	} else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}
	
	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}

static void error(const char* message) {
	errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
	errorAt(&parser.current, message);
}

static void advance() {
	parser.previous = parser.current;
	
	for (;;) {
		parser.current = scanToken();
		if(parser.current.type != TOKEN_ERROR) break;
		
		errorAtCurrent(parser.current.start);
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

static void emitReturn() {
	emitByte(OP_RETURN);
}
/*
static uint8_t makeConstant(Value value) {
	int constant = addConstant(currentChunk(), value);
	return (uint8_t)constant;
}*/

static void emitConstant(Value value) {
	writeConstant(currentChunk(), value, parser.previous.line);
}

static void initCompiler(Compiler* compiler) {
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	current = compiler;
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

static void endCompiler() {
	emitReturn();
#ifdef DEBUG_PRINT_CODE
	if(!parser.hadError) {
		disassembleChunk(currentChunk(), "code");
	}
#endif
}

static void beginScope() {
	current->scopeDepth++;
}

static void endScope() {
	current->scopeDepth--;
	int popCount = 0;
	
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		popCount++;
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

static int identifierConstantDeclaration(Token* name, bool constant) {
	ObjString* objString = allocateString(false, name->start, name->length);

	if (tableSetGlobal(&vm.globalConstantIndex, &OBJ_KEY(objString), NUMBER_VAL(currentChunk()->constants.count))) {
		return addConstant(currentChunk(), OBJ_VAL(objString), constant);	
	} else {
		Value constantIndex;
		tableGet(&vm.globalConstantIndex, &OBJ_KEY(objString), &constantIndex);
		if (currentChunk()->constants.values[(int)AS_NUMBER(constantIndex)].constant != constant) {
			error("Attempt to re-declare variable type qualifier.");
		}
		return (int)AS_NUMBER(constantIndex);
	}
}

static int identifierConstantSetGet(Token* name) {
	ObjString* objString = allocateString(false, name->start, name->length);
	Value constantIndex;
	tableGet(&vm.globalConstantIndex, &OBJ_KEY(objString), &constantIndex);
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
	
	return -1;
}

static void addLocal(Token name, bool constant) {
	if (current->localCount == SCOPE_COUNT) {
		error("Too many local variables in function.");
		return;
	}
	
	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
	local->constant = constant;
}

static void declareVariable(bool constant) {
	// Global variables are implicitly declared.
	if (current->scopeDepth == 0) return;
	
	Token* name = &parser.previous;
	
	for (int i = current->localCount - 1; i >= 0; i--) {
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}
		
		if (identifiersEqual(name, &local->name)) {
			if (local->constant != constant) {
				error("Attempt to re-declare variable type qualifier.");
			}
			error("Variable re-definition within scope.");
		}
	}
	
	
	addLocal(*name, constant);
}

static int parseVariable(const char* errorMessage, bool constant) {
	consume(TOKEN_IDENTIFIER, errorMessage);
	
	declareVariable(constant);
	if (current->scopeDepth > 0) return 0;
	
	return identifierConstantDeclaration(&parser.previous, constant);
}

static int markInitialized() {
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(int global) {
	if (current->scopeDepth > 0) {
		markInitialized();
		return;
	}
	
	emitOpAndConstant(OP_DEFINE_GLOBAL, global);
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
		case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
		case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
		default:
			return;
	}
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


static void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
	emitConstant(OBJ_VAL(allocateString(false, parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);
	bool global;
	if (arg != -1) {
		global = false;
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else {
		global = true;
		arg = identifierConstantSetGet(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}
	
	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		if (currentChunk()->constants.values[arg].constant == true && global) {
			error("Attempt to re-assign variable declared with type qualifier 'Const'.");
		} else if (current->locals[arg].constant = true && !global) {
			error("Attempt to re-assign variable declared with type qualifier 'Const'.");
		} else emitOpAndConstant(setOp, (uint8_t)arg);
	} else {
		emitOpAndConstant(getOp, (uint8_t)arg);
	}
}

static void variable(bool canAssign) {
	namedVariable(parser.previous, canAssign);
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

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN]= {grouping,NULL,PREC_NONE},
	[TOKEN_RIGHT_PAREN]= {NULL,NULL,PREC_NONE},
	[TOKEN_LEFT_BRACE]= {NULL,NULL,PREC_NONE},
	[TOKEN_RIGHT_BRACE]= {NULL,NULL,PREC_NONE},
	[TOKEN_COMMA]= {NULL,NULL,PREC_NONE},
	[TOKEN_DOT]= {NULL,NULL,PREC_NONE},
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
	[TOKEN_AND]= {NULL,NULL,PREC_NONE},
	[TOKEN_CLASS]= {NULL,NULL,PREC_NONE},
	[TOKEN_ELSE]= {NULL,NULL,PREC_NONE},
	[TOKEN_FALSE] = {literal, NULL, PREC_NONE},
	[TOKEN_FOR]= {NULL,NULL,PREC_NONE},
	[TOKEN_DEF]= {NULL,NULL,PREC_NONE},
	[TOKEN_IF]= {NULL,NULL,PREC_NONE},
	[TOKEN_NULL]= {literal,NULL,PREC_NONE},
	[TOKEN_OR]= {NULL,NULL,PREC_NONE},
	[TOKEN_PRINT]= {NULL,NULL,PREC_NONE},
	[TOKEN_RETURN]= {NULL,NULL,PREC_NONE},
	[TOKEN_BASE]= {NULL,NULL,PREC_NONE},
	[TOKEN_THIS]= {NULL,NULL,PREC_NONE},
	[TOKEN_TRUE]= {literal,NULL,PREC_NONE},
	[TOKEN_VAR]= {NULL,NULL,PREC_NONE},
	[TOKEN_WHILE]= {NULL,NULL,PREC_NONE},
	[TOKEN_ERROR]= {NULL,NULL,PREC_NONE},
	[TOKEN_EOF]= {NULL,NULL,PREC_NONE},
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

static void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}
	
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration(bool constant) {
	int global = parseVariable("Expect variable name.", constant);
	
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

static void printStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
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

static void declaration() {
	if (match(TOKEN_VAR)) {
		varDeclaration(false);
	} else {
		statement();
	}
	
	if (parser.panicMode) synchronize();
}

static void constDeclaration() {
	varDeclaration(true);
}

static void statement() {
	if(match(TOKEN_PRINT)) {
		printStatement();
	} else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	} else if (match(TOKEN_CONST)) {
		constDeclaration();
	} else {
		expressionStatement();
	}
}

bool compile(const char* source, Chunk* chunk) {
	initScanner(source);
	Compiler compiler;
	initCompiler(&compiler);
	compilingChunk = chunk;
	
	parser.hadError = false;
	parser.panicMode = false;
	advance();
	
	while (!match(TOKEN_EOF)) {
		declaration();
	}
	
	endCompiler();
	return !parser.hadError;
}

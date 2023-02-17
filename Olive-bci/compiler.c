#include <stdio.h>
#include <stdlib.h>

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

typedef void (*ParseFn)();

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

Parser parser;

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
	
	if (type == TOKEN_SEMICOLON) {
		fprintf(stderr, "[line %d] Error at end: Expect ';' after variable declaration\n", parser.current.line - 1);
		parser.hadError = true;
		return;
	}
		
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

static void emitGlobalConstant(uint8_t byte, int constant) {
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

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static int identifierConstant(Token* name) {
	return addConstant(currentChunk(), OBJ_VAL(allocateString(false, name->start, name->length)));
}

static int parseVariable(const char* errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);
	return identifierConstant(&parser.previous);
}

static void defineVariable(int global) {
	emitGlobalConstant(OP_DEFINE_GLOBAL, global);
}

static void ternary() {
	// Parse then branch
	parsePrecedence(PREC_TERNARY);
	
	consume(TOKEN_COLON, "Expect ':' after then branch of ternary operator.");
	
	// Parse else branch
	parsePrecedence(PREC_ASSIGNMENT);
	emitByte(OP_TERNARY);	
}

static void binary() {
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

static void literal() {
	switch(parser.previous.type) {
		case TOKEN_FALSE: emitByte(OP_FALSE); break;
		case TOKEN_NULL: emitByte(OP_NULL); break;
		case TOKEN_TRUE: emitByte(OP_TRUE); break;
		default:
			return;
	}
}


static void grouping() {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number() {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void string() {
	emitConstant(OBJ_VAL(allocateString(false, parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name) {
	int arg = identifierConstant(&name);
	emitGlobalConstant(OP_GET_GLOBAL, arg);
}

static void variable() {
	namedVariable(parser.previous);
}

static void unary() {
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
	
	prefixRule();
	
	while(precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule();
	}
}

static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

static void expression() {
	parsePrecedence(PREC_ASSIGNMENT);
}

static void varDeclaration() {
	int global = parseVariable("Expect variable name.");
	
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
	emitByte(OP_ADD);
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
		varDeclaration();
	} else {
		statement();
	}
	
	if (parser.panicMode) synchronize();
}

static void statement() {
	if(match(TOKEN_PRINT)) {
		printStatement();
	} else {
		expressionStatement();
	}
}

bool compile(const char* source, Chunk* chunk) {
	initScanner(source);
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

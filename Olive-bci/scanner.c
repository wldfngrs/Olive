#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

static int interpolationCount = 0;
static bool inInterpolation = false;
static bool interpolatedString = false;
static bool jsi = false; // jsi -- justScannedInterpolation. Set when the scanner has scanned passed all the interpolation tokens in a given string.

typedef struct {
	const char* start;
	const char* current;
	int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}

static bool isAlpha(char c) {
	return	(c >= 'a' && c <= 'z')||
		(c >= 'A' && c <= 'Z')||
		(c == '_');
}

static bool isDigit(char c) {
	return c >= '0' && c <= '9';
}

static bool isAtEnd() {
	return *scanner.current == '\0';
}

static char advance() {
	scanner.current++;
	return scanner.current[-1];
}

static char peek() {
	return *scanner.current;
}

static char peekNext() {
	if (isAtEnd()) return '\0';
	return scanner.current[1];
}

static bool endMLComment() {	// end multi-line comment
	if (*scanner.current == '*' && *(scanner.current + 1) == '/') {
		return true;
	} else false;
}

static bool match(char expected) {
	if(isAtEnd()) return false;
	if(*scanner.current != expected) return false;
	
	scanner.current++;
	return true;
}

static Token makeToken(TokenType type) {
	Token token;
	token.type = type;
	token.start = scanner.start;
	token.length = (int)(scanner.current - scanner.start);
	token.line = scanner.line;
	return token;
}

static Token makeConcatToken(TokenType type) {
	Token token;
	token.type = type;
	token.start = NULL;
	token.length = 0;
	token.line = scanner.line;
	return token;
}

static Token errorToken(const char* message) {
	Token token;
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int)strlen(message);
	token.line = scanner.line;
	
	return token;
}

bool newLine = false;

static void skipWhitespace() {
	if (inInterpolation) return;
		
	for(;;) {
		char c = peek();
		switch(c) {
			case ' ':
			case '\r':
			case '\t':
				advance();
				break;
			case '\n':
				// change made here
				newLine = true;
				return;
			case '/':
				if (peekNext() == '/') {
					while(peek() != '\n' && !isAtEnd()) advance();
					break;
				} else if (peekNext() == '*') {
					advance(); //skip past '/'
					advance(); // skip past '*'
					while(!endMLComment() && !isAtEnd()) {
						if (peek() == '\n') {
							scanner.line++;
						}
						advance();
					}
					advance(); // skip past '*'
					advance(); // skip past '/'
					break;
				}else {
					return;
				}
			default:
				return;
		}
	}
}

static TokenType checkKeyword(int start, int length, const char* rest, TokenType type) {
	if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
		return type;
	}
	
	return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
	switch(scanner.start[0]) {
		case 'a': return checkKeyword(1,2, "nd", TOKEN_AND);
		case 'b': 
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'a': return checkKeyword(2,2, "se", TOKEN_BASE);
					case 'r': return checkKeyword(2,3, "eak", TOKEN_BREAK);
				}
			}
			break;
		return checkKeyword(1,3, "ase", TOKEN_BASE);
		case 'c': 
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'o': {
						if ((scanner.current - scanner.start) == 5) {
							return checkKeyword(2,3,"nst", TOKEN_CONST);	
						} else if ((scanner.current - scanner.start) == 8) {
							return checkKeyword(2,6, "ntinue", TOKEN_CONTINUE);
						}
					}
					case 'l': return checkKeyword(2,3, "ass", TOKEN_CLASS);
					case 'a': return checkKeyword(2,2, "se", TOKEN_SWITCHCASE);
				}
			}
			break;
		case 'd':
			if ((scanner.current - scanner.start) == 3) {
				return checkKeyword(1,2, "ef", TOKEN_DEF);
			} else if ((scanner.current - scanner.start) == 7) {
				return checkKeyword(1,6, "efault", TOKEN_SWITCHDEFAULT);
			} else if ((scanner.current - scanner.start) == 8) {
				return checkKeyword(1,7, "el_attr", TOKEN_DELATTR);
			}
			break;
		case 'e': return checkKeyword(1,3, "lse", TOKEN_ELSE);
		case 'f':
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'a': return checkKeyword(2,3,"lse", TOKEN_FALSE);
					case 'o': return checkKeyword(2,1,"r", TOKEN_FOR);
				}
			}
			break;
		case 'i': return checkKeyword(1,1, "f", TOKEN_IF);
		case 'n': return checkKeyword(1,3, "ull", TOKEN_NULL);
		case 'O': return checkKeyword(1,1, "or", TOKEN_OR);
		case 'p': return checkKeyword(1,4, "rint", TOKEN_PRINT);
		case 'r': return checkKeyword(1,5, "eturn", TOKEN_RETURN);
		case 's': return checkKeyword(1,5, "witch", TOKEN_SWITCH);
		case 't':
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'h': return checkKeyword(2,2,"is", TOKEN_THIS);
					case 'r': return checkKeyword(2,2,"ue", TOKEN_TRUE);
				}
			}
			break;
		case 'v': return checkKeyword(1,2, "ar", TOKEN_VAR);
		case 'w': return checkKeyword(1,4, "hile", TOKEN_WHILE);	
	}
	
	return TOKEN_IDENTIFIER;
}

static Token interpolation() {
	Token token = makeToken(TOKEN_INTERPOLATION);
	scanner.current+=1;
	return token;
}

static Token string() {	
	while(peek() != '"' && !isAtEnd()) {
		if(peek() == '\n') scanner.line++;
		if(peek() == '$' && peekNext() == '{') {
			interpolatedString = true;
			inInterpolation = true;
			return interpolation();
		}
		advance();
	}
	
	if(isAtEnd()){
		return errorToken("Unterminated string.");
	}
	
	advance();
	
	if(!interpolationCount) {
		interpolatedString = false;
		inInterpolation = false;
	}
	
	return makeToken(TOKEN_STRING);
}

static Token identifier() {
	while(isAlpha(peek()) || isDigit(peek())) advance();
	
	return makeToken(identifierType());
}

static Token number() {
	while(isDigit(peek())) advance();
	
	// Check if the number is decimal	
	if (peek() == '.' && isDigit(peekNext())) {
		// consume the '.'
		advance();
		
		while (isDigit(peek())) advance();
	}
	
	return makeToken(TOKEN_NUMBER);
}


// TODO: scan user enforced new line characters. That is, when the user types '\' and 'n'.
Token scanToken() {
	skipWhitespace();
	if (newLine) {
		newLine = false;
		Token newLineToken = makeToken(TOKEN_NEWLINE);
		scanner.line++;
		advance();
		return newLineToken;
	}
	
	scanner.start = scanner.current;
	
	if (isAtEnd()) return makeToken(TOKEN_EOF);
	
	char c = advance();
	
	if (isAlpha(c)) return identifier();
	if (isDigit(c)) return number();
	
	switch(c) {
		case '\n': {
			if (inInterpolation) {
				inInterpolation = false;
				return scanToken();
			}
		}
		case ' ': {
			if (inInterpolation) return string();
			break;
		}
		case '(': return makeToken(TOKEN_LEFT_PAREN);
		case ')': return makeToken(TOKEN_RIGHT_PAREN);
		case '{': {
			if (interpolatedString) {
				inInterpolation = false;
				interpolationCount++;
				return makeConcatToken(TOKEN_CONCAT);
			}
			
			return makeToken(TOKEN_LEFT_BRACE);
		}
		
		case '}': { 
			if (interpolatedString) {
				inInterpolation = true;
				interpolationCount--;
				return makeConcatToken(TOKEN_CONCAT);
			}
			
			return makeToken(TOKEN_RIGHT_BRACE);
		}
		
		case ';': return makeToken(TOKEN_SEMICOLON);
		case ',': return makeToken(TOKEN_COMMA);
		case '.': return makeToken(TOKEN_DOT);
		case '-': return makeToken(TOKEN_MINUS);
		case '+': return makeToken(TOKEN_PLUS);
		case '/': return makeToken(TOKEN_SLASH);
		case '*': return makeToken(TOKEN_STAR);
		case '%': return makeToken(TOKEN_MOD);
		case '!':
			return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
		case '=':
			return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
		case '<':
			return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
		case '>':
			return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
		case '"':
			return string();
		case '?':
			return makeToken(TOKEN_QUESTION_MARK);
		case ':':
			return makeToken(TOKEN_COLON);
	}
	
	return errorToken("Unexpected character.");
}

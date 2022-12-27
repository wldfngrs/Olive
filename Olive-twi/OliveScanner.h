#ifndef olivescanner_header
#define olivescanner_header


#include <string>
#include <vector>
#include <map>
#include "Token.h"
#include <any>

struct Olive {
	Olive();
	
	~Olive() {
		delete interpreter;
	}
	
	void main(int argc, char** argv);
	void error (int line, std::string message);
	void report (int line, std::string where, std::string message);
	void error (int line, std::string message, char c);
	void error (Token token, std::string message);
	void runtimeError(RuntimeError error);

	bool hadError = false;
	bool hadRuntimeError = false;
private:
	static Interpreter interpreter = new Interpreter;
	std::string bytes;
	void runFile(std::string path);
	void runPrompt();
	void run (std::string source);
};

Olive olive;

/*
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/
struct Scanner {
	friend class Olive;

	Scanner(std::string source);
	Scanner();
	std::vector<Token> scanTokens();
	
private:
	size_t start{};
	size_t current{};
	bool isAtEnd();
	void scanToken();
	void identifier();
	void number();
	void string();
	void character();
	bool match (char expected);
	char peek();
	char peekNext();
	bool isAlpha(char c);
	bool isAlphaNumeric(char c);
	bool isDigit(char c);
	char advance();
	void addToken(TokenType type);
	void addToken(TokenType type, Object literal);
	std::map<std::string, TokenType> keywords {
		{"and", TokenType::AND},
		{"class", TokenType::CLASS},
		{"else", TokenType::ELSE},
		{"false", TokenType::FALSE},
		{"for", TokenType::FOR},
		{"fun", TokenType::FUN},
		{"if", TokenType::IF},
		{"nil", TokenType::NIL},
		{"or", TokenType::OR},
		{"print", TokenType::PRINT},
		{"return", TokenType::RETURN},
		{"super", TokenType::SUPER},
		{"this", TokenType::THIS},
		{"true", TokenType::TRUE},
		{"var", TokenType::VAR},
		{"while", TokenType::WHILE},
	};
	
	// KINDA HACKY BUT LOOKS GOOD SO...
	std::map<std::string, bool> booleanValues {
		{"false", false},
		{"true", true}
	};
	
	std::string source;
	std::vector<Token> tokens;
	size_t line{1};
	bool scannerError{false};
};
#endif

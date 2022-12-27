#include "OliveScanner.h"
#include "Interpreter.h"
#include "Parser.h"
#include <fstream>
#include <iostream>

Olive::Olive() {}

void Olive::main(int argc, char** argv) {
	std::cin.exceptions(std::istream::badbit);
	if (argc > 2) {
		std::cout << "Usage: olive [script]\n";
		exit(0);
	} else if (argc == 2) { // Run file in a specified directory
		runFile(argv[1]);
	} else if (argc == 1){ // Run prompt
		runPrompt();
	}
}

void Olive::error (int line, std::string message) {
	report(line, "", message);
}

void Olive::error (Token token, std::string message) {
	if (token.type == TokenType::eof) {
		report(token.line, " at end", message);
	} else {
		std::string where{" at \'"};
		where.append(token.lexeme);
		where.append("\'");
		report(token.line, where, message);
	}
}

void Olive::error (int line, std::string message, char c) {
	message.append(" ");
	message.append("\'");
	message.push_back(c);
	message.append("\'");
	report(line, "", message);
}

void Olive::runtimeError(RuntimeError error) {
	std::cout << error.message << "\n[line " << error.token.line << "]" << std::endl;
	hadRuntimeError = true;
}

void Olive::report (int line, std::string where, std::string message) {
	std::cerr << "[line " << line << "] Error" << where << ": " << message << std::endl;
	hadError = true;
}

void Olive::runFile(std::string path) {
	std::ifstream iFileStream;
	iFileStream.open(path, std::ios::binary | std::ios::in);
	if (iFileStream.fail()) {
		throw std::system_error(errno, std::system_category());
	}
	
	char byte;
	while (iFileStream.get(byte)) {		
		bytes.push_back(byte);
	}
	
	iFileStream.close();
	
	run(bytes);
		
	if (hadError) {
		exit(10); // exit code for scanner/parser error
	}
	if (hadRuntimeError) {
		exit(15); // exit code for runtime error
	}
}

void Olive::runPrompt() {
	std::string line;
	std::cout << ">> ";
	while (std::cin.peek() != EOF) {
		getline(std::cin, line);
		if (line == "exit" || line == "quit") {
			std::cout << "Quit.\n";
			exit(0);
		}
		run(line);
		std::cout << ">> ";
		hadError = false;
	}
}

void Olive::run (std::string source) {
	Scanner scanner;
	scanner.source = source;
	std::vector<Token> tokens = scanner.scanTokens();
		
	Parser parser{tokens};
	Expr* expression {parser.parse()};
	
	if (hadError) return;
	
	interpreter.interpret(expression);
}

extern Olive olive;
/*
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

bool isDouble{false};

Scanner::Scanner(std::string source) 
	: source{source}
{}

Scanner::Scanner() {}

std::vector<Token> Scanner::scanTokens() {
	while(!isAtEnd()) {
		start = current;
		scanToken();
	}
	
	Token token{TokenType::eof, "", new doubleObject{NULL}, line};
	
	tokens.push_back(token);
	return tokens;
}

bool Scanner::isAtEnd() {
	return current >= source.length();
}

void Scanner::scanToken() {
	char c = advance();
	switch(c) {
		case '(': addToken(TokenType::LEFT_PAREN); break;
		case ')': addToken(TokenType::RIGHT_PAREN); break;
		case '{': addToken(TokenType::LEFT_BRACE); break;
		case '}': addToken(TokenType::RIGHT_BRACE); break;
		case ',': addToken(TokenType::COMMA); break;
		case '.': addToken(TokenType::DOT); break;
		case '-': addToken(TokenType::MINUS); break;
		case '+': addToken(TokenType::PLUS); break;
		case ';': addToken(TokenType::SEMICOLON); break;
		case '*': addToken(TokenType::STAR); break;
		case '?': addToken(TokenType::QUESTION_MARK); break;
		case ':': addToken(TokenType::COLON); break;
		case '!':
			addToken(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
			break;
		case '=':
			addToken(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
			break;
		case '<':
			addToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
			break;
		case '>':
			addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
			break;
		case '/':
			if (match('/')) {
				while (peek() != '\n' && !isAtEnd()) advance();
			} else if (match('*')) {
				while (peek() != '*') {
					if (advance() == '\n'){
						line++;
					}
				}
				
				advance();
				advance();
			} else {
				addToken(TokenType::SLASH);
			}
			break;
		case ' ':
		case '\r':
		case '\t':
			// ignore whitespaces
			break;	
		case '\n':
			line++;
			break;
		case '"': 
			string();
			break;
		case '\'':
			character();
			break;
		default:
			if (isDigit(c)) {
				number();
			} else if (isAlpha(c)) {
				identifier();
			} else {
				scannerError = true;
				olive.error(line, "Unexpected character", c);
			}
			break;
	}
}

void Scanner::identifier() {
	while (isAlphaNumeric(peek())) advance();
	
	TokenType type;
	bool boolean;
	std::string str{source, start, current-start};
	try {
		type = keywords.at(str);
	} catch (std::exception &e) {
		type = TokenType::IDENTIFIER;
		addToken(type, new stringObject {str});
		return;
	}
	
	try {
		boolean = booleanValues.at(str);	
	} catch (std::exception &e) {
		return;
	}

	addToken(type, new boolObject{boolean});
}

void Scanner::number() {
	while (isDigit(peek())) advance();
	
	// Look for a fractional part.
	if (peek() == '.' && isDigit(peekNext())) {
		// consume the '.'
		advance();
		isDouble = true;
		
		// Consume rest of fractional part
		while (isDigit(peek())) advance();
	}
	
	std::string str{source, start, current-start};
	if (isDouble == true) {
		isDouble = false;
		addToken(TokenType::NUMBER, new doubleObject{std::stod({str})});
	}
	else {
		int a = std::stoi({str});
		addToken(TokenType::NUMBER, new intObject{a});
	}
}

void Scanner::string() {
	while (peek() != '"' && !isAtEnd()) {
		if (peek() == '\n') line++;
		advance();
	}
	
	if (isAtEnd()) {
		olive.error(line, "Unterminated string.");
		return;
	}
	
	// The closing ".
	advance();
	
	// Trim the surrounding quotes
	std::string value{source, start, current-start};
	addToken(TokenType::STRING, new stringObject{value});
}

void Scanner::character() {
	char character;
	while (peek() != '\'' && !isAtEnd()) {
		if(peek() == '\n') line++; //TODO:: RECHECK HERE
		character = peek();
		advance();
	}
	
	if (isAtEnd()) {
		olive.error(line, "Missing terminating ' character");
	}
	
	// The closing '.
	advance();
	addToken(TokenType::CHAR, new charObject{character});
}

bool Scanner::match (char expected) {
	if (isAtEnd()) return false;
	if (source.at(current) != expected) return false;
		
	current++;
	return true;
}

char Scanner::peek() {
	if (isAtEnd()) return '\0';
	return source.at(current);
}
	
char Scanner::peekNext() {
	if (current + 1 >= source.length()) return '\0';
	return source.at(current+1);
}
	
bool Scanner::isAlpha(char c) {
	return (c >= 'a' && c <= 'z') ||
	       (c >= 'A' && c <= 'Z') ||
	       (c == '_');
}
	
bool Scanner::isAlphaNumeric(char c) {
	return isAlpha(c) || isDigit(c);
}
	
bool Scanner::isDigit(char c) {
	return c >= '0' && c <= '9';
}
	
char Scanner::advance() {
	return source.at(current++);
}
	
void Scanner::addToken(TokenType type) {
	addToken(type, new doubleObject{NULL});
}
	
void Scanner::addToken(TokenType type, Object literal) {
	std::string text{source, start, current - start};
	Token token{type, text, literal, line};
	tokens.push_back(token);
}

#ifndef parser_header
#define parser_header


#include "OliveScanner.h"
#include "Expr.h"
#include "Token.h"
#include "TokenType.h"

// delete all the objects that were created within this class

extern Olive olive;

struct Parser {
	Parser(std::vector<Token> tokens)
		: tokens{tokens}
	{}
	
	Expr* parse() {
		try {
			return expression();
		} catch (std::runtime_error) {
			return NULL;
		}
	}

private:
	Expr* expression() {
		return comma();
	}
	
	Expr* comma() {
		Expr* expr = ternary();
		
		std::vector<TokenType> InitializerList{
			TokenType::COMMA
		};
		
		while (match(InitializerList)) {
			Token oprtr = previous(); // get the comma operator
			Expr* right = ternary(); // get right operand
			
			expr = new Binary{expr, oprtr, right};
		}
		
		return expr;
	}
	
	Expr* ternary() {
		Expr* expr = equality();
		
		if (check(TokenType::QUESTION_MARK)) {
			advance();
			Token question_oprtr = previous(); // get the question mark operator
			Expr* rightOfQuestionMark = ternary();
			expr = new Binary{expr, question_oprtr, rightOfQuestionMark};
		}
		
		if (check(TokenType::COLON)) {
			advance();
			Token colon_oprtr = previous(); // get the colon operator
			Expr* rightOfColon = ternary();
			expr = new Binary {expr, colon_oprtr, rightOfColon};
		}
		
		return expr;
	}
	
	Expr* equality() {
		Expr* expr = comparison();
		
		std::vector<TokenType> InitializerList{
			TokenType::BANG_EQUAL,
			TokenType::EQUAL_EQUAL
		};
		
		while (match(InitializerList)) {
			Token oprtr = previous(); // get the operator
			Expr* right = comparison(); // get right operand
			expr = new Binary{expr, oprtr, right};
		}
		
		return expr;
	}
	
	Expr* comparison() {
		Expr* expr = term();
		
		std::vector<TokenType> InitializerList{
			TokenType::GREATER, 
			TokenType::GREATER_EQUAL, 
			TokenType::LESS,
			TokenType::LESS_EQUAL
		};
		
		while (match(InitializerList)) {
			Token oprtr = previous();
			Expr* right = term();
			expr = new Binary{expr, oprtr, right};
		}
		
		return expr;
	}
	
	Expr* term() {
		Expr* expr = factor();
		
		std::vector<TokenType> InitializerList{
			TokenType::MINUS,
			TokenType::PLUS
		};
		
		while (match(InitializerList)) {
			Token oprtr = previous();
			Expr* right = factor();
			expr = new Binary{expr, oprtr, right};
		}
		
		return expr;
	}
	
	Expr* factor() {
		Expr* expr = unary();
		
		std::vector<TokenType> InitializerList{
			TokenType::SLASH,
			TokenType::STAR
		};
		
		while (match(InitializerList)) {
			Token oprtr = previous();
			Expr* right = unary();
			expr = new Binary{expr, oprtr, right};
		}
		
		return expr;
	}
	
	Expr* unary() {
		std::vector<TokenType> InitializerList {
			TokenType::BANG,
			TokenType::MINUS
		};
		
		if (match(InitializerList)) {
			Token oprtr = previous();
			Expr* right = unary();
			right = new Unary{oprtr, right};
			return right;
		}
		
		return primary();
	}
	
	Expr* primary() {
		std::vector<std::vector<TokenType>> ListOfInitializerLists {
			{TokenType::FALSE},
			{TokenType::TRUE},
			{TokenType::NIL},
			{TokenType::NUMBER,
			 TokenType::STRING},
			{TokenType::LEFT_PAREN}
		};
		
		if(match(ListOfInitializerLists[0])) {
			Token prevToken = previous();
			Expr* expr = new Literal{prevToken.literal};
			return expr;
		}
		
		if (match(ListOfInitializerLists[1])) {
			Token prevToken = previous();
			Expr* expr = new Literal{prevToken.literal};
			return expr;
		}
		
		if (match(ListOfInitializerLists[2])) {
			Token prevToken = previous();
			Expr* expr = new Literal{prevToken.literal};
			return expr;
		}
		
		if (match(ListOfInitializerLists[3])) {
			Token prevToken = previous();
			Expr* expr = new Literal{prevToken.literal};
			return expr;
		}
		
		if (match(ListOfInitializerLists[4])) {
			Expr* expr = expression();
			consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
			expr = new Grouping{expr};
			return expr;
		} 
		
		else {
			error(peek(), "Expect expression.");
		}
	}
	
	bool match(std::vector<TokenType> types) {
		for (TokenType type : types) {
			if (check(type)) {
				advance();
				return true;
			}
		}
		
		return false;
	}
	
	//TODO: change return to void in case there's
	//never a need for the Token instance it returns
	Token consume(TokenType type, std::string message) {
		if (check(type)) {
			return advance();
		} else error(peek(), message);
	}
	
	bool check(TokenType type) {
		if (isAtEnd()) return false;
		return peek().type == type;
	}
	
	Token advance() {
		if (!isAtEnd()) current++;
		return previous();
	}
	
	bool isAtEnd() {
		return peek().type == TokenType::eof;
	}
	
	Token peek() {
		return tokens[current];
	}
	
	Token previous() {
		return tokens[current - 1];
	}
	
	void error(Token token, std::string message) {
		olive.error(token, message);
		throw std::runtime_error{message};
	}
	
	void synchronize() {
		advance();
		
		while (!isAtEnd()) {
			Token prevToken = previous();
			if(prevToken.type == TokenType::SEMICOLON) return;
			
			switch(peek().type) {
				case TokenType::CLASS:
				case TokenType::FUN:
				case TokenType::VAR:
				case TokenType::FOR:
				case TokenType::IF:
				case TokenType::WHILE:
				case TokenType::PRINT:
				case TokenType::RETURN:
					return; 
			}
			
			advance();
		}
	}
	
	//struct ParseError : std::runtime_error {};
	
	std::vector<Token> tokens;
	size_t current = 0;
};
#endif

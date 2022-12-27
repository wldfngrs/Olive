#ifndef tokentype_header
#define tokentype_header


#include <map>
#include <string>

enum class TokenType {
	//single character tokens.
	LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
	COMMA, DOT, MINUS, PLUS, SEMICOLON, SLASH, STAR, COLON, QUESTION_MARK,
	
	// One or two character tokens.
	BANG, BANG_EQUAL,
	EQUAL, EQUAL_EQUAL,
	GREATER, GREATER_EQUAL,
	LESS, LESS_EQUAL,
	// TODO: Implement for &, &&, |, || operators. Just for the kick of it tbh
	
	// literals
	IDENTIFIER, STRING, CHAR, NUMBER,
	
	// Keywords
	AND, CLASS, ELSE, FALSE, FUN, FOR, IF, NIL, OR,
	PRINT, RETURN, SUPER, THIS, TRUE, VAR, WHILE, eof
};

std::map<TokenType, std::string> TokenTypeStrings{
	{TokenType::LEFT_PAREN, "LEFT_PAREN"},
	{TokenType::RIGHT_PAREN, "RIGHT_PAREN"},
	{TokenType::LEFT_BRACE, "LEFT_BRAC"},
	{TokenType::RIGHT_BRACE, "RIGHT_BRACE"},
	{TokenType::COMMA, "COMMA"},
	{TokenType::DOT, "DOT"},
	{TokenType::MINUS, "MINUS"},
	{TokenType::PLUS, "PLUS"},
	{TokenType::SEMICOLON, "SEMICOLON"}, 
	{TokenType::SLASH, "SLASH"},
	{TokenType::STAR, "STAR"},
	{TokenType::COLON, "COLON"},
	{TokenType::QUESTION_MARK, "QUESTION_MARK"},
	{TokenType::BANG, "BANG"},
	{TokenType::BANG_EQUAL, "BANG_EQUAL"},
	{TokenType::EQUAL, "EQUAL"},
	{TokenType::EQUAL_EQUAL, "EQUAL_EQUAL"},
	{TokenType::GREATER, "GREATER"},
	{TokenType::GREATER_EQUAL, "GREATER_EQUAL"},
	{TokenType::LESS, "LESS"},
	{TokenType::LESS_EQUAL, "LESS_EQUAL"},
	{TokenType::IDENTIFIER, "IDENTIFIER"},
	{TokenType::STRING, "STRING"},
	{TokenType::NUMBER, "NUMBER"},
	{TokenType::AND, "AND"},
	{TokenType::CLASS, "CLASS"},
	{TokenType::ELSE, "ELSE"},
	{TokenType::FALSE, "FALSE"},
	{TokenType::FUN, "FUN"},
	{TokenType::FOR, "FOR"},
	{TokenType::IF, "IF"},
	{TokenType::NIL, "NIL"},
	{TokenType::OR, "OR"},
	{TokenType::PRINT, "PRINT"},
	{TokenType::RETURN, "RETURN"},
	{TokenType::SUPER, "SUPER"},
	{TokenType::THIS, "THIS"},
	{TokenType::TRUE, "TRUE"},
	{TokenType::VAR, "VAR"},
	{TokenType::WHILE, "WHILE"},
	{TokenType::eof, "eof"}
};
#endif

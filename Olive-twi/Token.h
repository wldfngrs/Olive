#ifndef token_header
#define token_header


#include <ostream>
#include <string>
#include <sstream>
#include "TokenType.h"
#include "Object.h"

template<typename T>
struct Token {
	Token() = default;
	
	Token(const TokenType type, const std::string lexeme, const Object<T> literal, const size_t line)
		: type{type}, lexeme{lexeme}, literal{literal}, line{line}
	{}

	/*std::string toString() {
		std::string result;
		std::ostringstream ss;
		ss << TokenTypeStrings.at(type) << " " << lexeme << " " << literal->ObjectToString() << " " << line;
		result = ss.str();
		return result;
	}*/
	
	const TokenType type;
	const std::string lexeme;
	const Object<T> literal;
	const size_t line;
};

/*
std::ostream& operator<<(std::ostream& t, Token token) {
	t << token;
	return t;
}

std::ostream& operator<<(std::ostream& s, TokenType type) {
	s << type;
	return s;
}

std::ostream& operator<<(std::ostream& h, Object obj) {
	h << obj;
	return h;
}*/
#endif

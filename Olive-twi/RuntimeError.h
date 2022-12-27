#include "Token.h"

struct RuntimeError : std::exception {
	RuntimeError(Token token, const char* message) {
		this->token = token;
		this->message = message;
	}
	
	const Token<double> token;
	const char* message;	
};

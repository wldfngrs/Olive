#ifndef expr_header
#define expr_header

#include "Token.h" // INCLUDES 'OBJECT.H' AND 'TOKENTYPE.H'

struct Binary;

struct Grouping;

template<typename T>
struct Literal;

struct Unary;

template<typename T>
struct visitorInterface {
	virtual T visitBinaryExpr(Binary& expr) = 0;
	virtual T visitGroupingExpr(Grouping& expr) = 0;
	virtual T visitLiteralExpr(Literal& expr) = 0;
	virtual T visitUnaryExpr(Unary& expr) = 0;
};

struct Expr {
	virtual T accept(visitorInterface<T>* visitor) = 0;
};

struct Binary : Expr {
	Binary (Expr* left, Token<double> oprtr, Expr* right) 
		: left{left}, oprtr{oprtr}, right{right}
	{}

	Expr* left;
	const Token<double> oprtr;
	Expr* right;

	std::string accept(visitorInterface<std::string>* visitor) override {
		return visitor->visitBinaryExpr(*this);
	}
};

struct Grouping : Expr {
	Grouping (Expr* expression) 
		: expression{expression}
	{}

	Expr* expression;

	std::string accept(visitorInterface<std::string>* visitor) override {
		return visitor->visitGroupingExpr(*this);
	}
};

template<typename T>
struct Literal : Expr {
	Literal (const Object<T> value) 
		: value{value}
	{}

	const Object<T> value;

	std::string accept(visitorInterface<std::string>* visitor) override {
		return visitor->visitLiteralExpr(*this);
	}
};

struct Unary : Expr {
	Unary (Token<double> oprtr, Expr* right) 
		: oprtr{oprtr}, right{right}
	{}

	const Token<double> oprtr;
	Expr* right;

	std::string accept(visitorInterface<std::string>* visitor) override {
		return visitor->visitUnaryExpr(*this);
	}
};
#endif

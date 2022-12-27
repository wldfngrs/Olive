#include "Expr.h"
#include <string>
#include <iostream>

struct AstPrinter : visitorInterface<std::string> {
	std::string print(Expr* expr) {
		return expr->accept(this);
	}
	
	std::string visitBinaryExpr(Binary& expr) override {
		return parenthesize(expr.oprtr.lexeme, expr.left, expr.right);
	}
	
	std::string visitGroupingExpr(Grouping& expr) override {
		return parenthesize("group", expr.expression);
	}
	
	std::string visitLiteralExpr(Literal<long>& expr) {
		if (expr.value.ObjectToString() == "0") {
			return "nil";
		}
		
		return expr.value.ObjectToString();
	}
	
	std::string visitLiteralExpr(Literal<double>& expr) {
		if (expr.value.ObjectToString() == "0") {
			return "nil";
		}
		
		return expr.value.ObjectToString();
	}
	
	std::string visitLiteralExpr(Literal<std::string>& expr) {
		if (expr.value.ObjectToString() == "0") {
			return "nil";
		}
		
		return expr.value.ObjectToString();
	}
	
	std::string visitLiteralExpr(Literal<char>& expr) {
		if (expr.value.ObjectToString() == "0") {
			return "nil";
		}
		
		return expr.value.ObjectToString();
	}
	
	std::string visitLiteralExpr(Literal<bool>& expr) {
		if (expr.value.ObjectToString() == "0") {
			return "nil";
		}
		
		return expr.value.ObjectToString();
	}
	
	std::string visitUnaryExpr(Unary& expr) override {	
		return parenthesize(expr.oprtr.lexeme, expr.right);
	}
	
private:
	std::string parenthesize(std::string name, Expr* expr) {
		std::string string;	// parenthesized string
		string.append("(");
		string.append(name);
		string.append(" ");
		string.append(expr->accept(this));	//	TODO: CHECK THIS LINE WHEN ERRORS OCCUR!
		string.append(")");
		
		return string;
	}
	
	std::string parenthesize(std::string lexeme, Expr* expr1, Expr* expr2) {
		std::string string;
		string.append("(");
		string.append(lexeme);
		string.append(" ");
		string.append(expr1->accept(this));
		string.append(" ");
		string.append(expr2->accept(this));
		string.append(")");
		
		return string;
	}	
};

int main() {
	AstPrinter ast;
	Object null = new doubleObject{NULL};
	Token<double> token{TokenType::MINUS, "-", null, 1};
	Token<double> token1{TokenType::STAR, "*", null, 1};
	Object<long> iObj = new intObject{0};
	Literal<long> literal{iObj};
	Unary unary{token, &literal};
	Object<double> dObj = new doubleObject{45.67};
	Literal<double> literal2{dObj};
	Grouping grouping{&literal2};
	Binary binary {
		&unary,
		token1,
		&grouping
	};
	Expr<std::string>* expression{&binary};
	std::cout << ast.print(expression) << std::endl;
}

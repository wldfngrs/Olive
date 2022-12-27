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
	
	std::string visitLiteralExpr(Literal& expr) override {
		if (expr.value.ObjectToString() == "0") {
			return "nil";
		}
		
		return expr.value.ObjectToString();
	}
	
	std::string visitUnaryExpr(Unary& expr) override{	
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

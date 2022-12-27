#ifndef interpreter_header
#define interpreter_header

#include "Expr.h"
#include "Object.h"
#include "OliveScanner.h"
#include "RuntimeError.h"
#include <stdexcept>

extern Olive olive;
struct Interpreter : visitorInterface<Object> {
	void interpret(Expr* expression) {
		try {
			Object value = evaluate(expression);
			std::cout << value.ObjectToString() << std::endl
		} catch (const RuntimeError error) {
			olive.runtimeError(error);
		}	
	}
	
	Object visitLiteralExpr(Literal& expr) override {
		return expr.value;
	}
	
	Object visitGroupingExpr(Grouping& expr) override {
		return evaluate(expr.expression);
	}
	
	Object visitUnaryExpr(Unary& expr) override {
		Object right = evaluate(expr.right);
		
		try {
			switch(expr.oprtr.type) {
				case TokenType::MINUS:
					return -right;
				case TokenType::BANG:
					return !isTruthy(right);
			}
		} catch (std::runtime_error& e) {
			throw new RuntimeError{expr.oprtr, e.what()};
		}
		return NULL;
	}
	
	Object visitBinaryExpr(Binary& expr) {
		Object left = evaluate(expr.left);
		Object right = evaluate(expr.left);
		
		try {
			switch (expr.oprtr.type) {
				case TokenType::GREATER:
					return left > right;
				case TokenType::GREATER_EQUAL:
					return left >= right;
				case TokenType::LESS:
					return left < right;
				case TokenType::LESS_EQUAL:
					return left <= right;
				case TokenType::MINUS:
					return left - right;
				case TokenType::SLASH:
					return left / right;
				case TokenType::STAR:
					return left * right;
				case TokenType::PLUS:
					return left + right;
				case TokenType::BANG_EQUAL:
					return !isEqual(left, right);	
				case TokenType::EQUAL_EQUAL:
					return isEqual(left, right);		
			}
		} catch (std::runtime_error& e) {
			throw new RuntimeError{expr.oprtr, e.what()};
		}
		return NULL;
	}
	
private:
	Object evaluate(Expr* expr) {
		return expr->accept(this);
	}
	
	// Following Ruby's rule for 'truth-y' and 'false-y' of values
	bool isTruthy(Object object) {
		if (object.type.value == NULL) return false;
		if (object.type.value == true) return true;	// for boolean Objects with a value of 'true'
		if (object.type.value == false) return false;	// for boolean Objects with a value of 'false'
		else return true; // for all other Object instances
	}
	
	bool isEqual(Object a, Object b) {
		if(a.type.value == NULL && b.type.value == NULL) return true;
		if(a.type.value == null) return false;
		
		return (a.type.value == b.type.value);
	}
}
#endif

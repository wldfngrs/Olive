#ifndef object_header
#define object_header

// change the name of private data members of the classes inheriting from objectType to 'value' to make it truly polymorphic(?)

#include <string>
#include <typeinfo>

template<typename T> 
struct Object {
	Object(T value) : value{value}
	{}
	
	std::string ObjectToString() {
		return std::to_string(value);
	}
	
	T ObjectToValue() {
		return value;
	}
	
private:
	T value;
};

template<>
struct Object<std::string> {
	Object(std::string value) : value{value}
	{}
	
	std::string ObjectToString() {
		return value;
	}
	
	std::string ObjectToValue() {
		return value;
	}
private:
	std::string value;
};

template<>
struct Object<char> {
	Object(char value) : value{value}
	{}
	
	char ObjectToString() {
		return value;
	}
	
	char ObjectToValue() {
		return value;
	}
private:
	char value;
};

#endif

// INT object to store all possible integer values. Underlying implementation is a 'long' to accomadate numbers exceeding the typical integer range in C++.
struct intObject : objectType<long> {
	intObject(long longArgument) 
		: value{longArgument}
	{}
	
	std::string ToString() const {
		return std::to_string(value);
	}
	
	auto ToValue() const override {
		return value;	
	}
	
private:
	long value;
};


// DOUBLE object to store all possible decimal values.
struct doubleObject : objectType<double> {
	doubleObject(double doubleArgument) 
		: value{doubleArgument}
	{}
	
	std::string ToString() const {
		return std::to_string(value);
	}
	
	auto ToValue() const override {
		return value;
	}
 	
private:
	double value;
};

// CHAR object to store ascii type characters;
struct charObject : objectType<char> {
	charObject (char charArgument)
		: value{charArgument}
	{}
	
	std::string ToString() const {
		return std::to_string(value);
	}
	
	auto ToValue() const override {
		return value;
	}
	
private:
	char value;
};

// STRING object to store std::string objects
struct stringObject : objectType<std::string> {
	stringObject (std::string stringArgument)
		: value{stringArgument}
	{}
	
	std::string ToString() const {
		return value;
	}
	
	auto ToValue() const override {
		return value;
	}
	
private:
	std::string value;
};

// BOOL object to store true or false objects
struct boolObject : objectType<bool>{
	boolObject (bool boolArgument) 
		: value{boolArgument}
	{}
	
	std::string ToString() const {
		return std::to_string(value);
	}
	
	auto ToValue() const {
		return value;
	}	
private:
	bool value;
};


double mydouble{1.0};
int myint{1};
std::string mystring{"a"};
char mychar{'a'};

// Using constructor injection to handle runtime polymorphism of the different 'Object' types.
struct Object {
	Object(objectType<T>* Type) 
		: type{Type} 
	{}
	
	std::string ObjectToString() const {
		return type->ToString();
	}
	
	T ObjectToValue() const {
		return type->ToValue();
	}
	
private:
	objectType<T>* type;
};

/*Object operator-(Object object) {
	/*switch(typeid(object.ObjectToValue()).name()) {
		case typeid(mydouble).name():
			return Object returnObj{new doubleObject{-(object.ObjectToValue())}};
		case typeid(myint).name():
			return Object returnObj{new intObject{-(object.ObjectToValue())}};
		default:
			throw std::runtime_error{"Invalid operand for '-' operator. Operand must be a number."};			
	}
	
	return NULL;
}*/
#endif

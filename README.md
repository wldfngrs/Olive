![olive_image](https://github.com/wldfngrs/Olive/blob/main/Olive-bci/asset/icon.png)
# Olive Interpreter
To be fair, a huge part of this repository is "inspired" by Robert Nystrom's 'Lox' as described in his book "Crafting Interpreters". Lox is, for all the right reasons, however a little too vanilla and skips over a few vital things. To spice things up, I've implemented an extension of other features found to be a staple in other heralded programming languages like Python, C and JavaScript. The end result is 'Olive'.

## Installation

Clone this repository to your local computer:

`$ git clone https://github.com/wldfngrs/Olive.git`

Navigate to the `Olive-bci` directory for the byte-coded interpreter and run the following command to create the executable file.

`$ make olive`

There! Now you have Olive set up.

## Syntax

To keep things rather simple and to significantly reduce onboarding time for the user, Olive features syntax rules present in established languages like C, Python and JavaScript. It should be noted that for both aesthetic and technical reasons, Olive enforces a semi-colon to end every statement or expression.

### Variables

Declare varibales using the `var` keyword:

```
var variable_name = 10;
```
And access them by their names as defined:

```
variable_name = variable_name * 2;
print variable_name; // 20
```
For constant variables, declare them using the `const` keyword;
```
const const_variable_name = 10;
```
And access them by their names as defined:
```
print const_variable_name; // 10
```
Take care not to attempt modifying them though. They were declared `const` for a reason:
```
const_variable_name = const_variable_name + 1; // error!
```

### Functions

I've always loved the `def` style of function declaration so declare functions in Olive using the  `def` keyword:

```
def function_name(argumen1, argument2, argument3) {
  // do something
}
```
And call defined functions this way;

```
function_name(argument1, argument2, argument3);
```

### User-defined classes

Declare user-defined classes using the `class` keyword:

```
class class_name {
  init() {
    // do something on class instantiation
  }
}
```
You can include an optional 'constructor' function `init()` that gets called when an instance of a class is created. Olive user-define classes also feature the `this` keyword which can be used to instantiate the class fields from the `init()` constructor or one of the class' defined methods. Note that attempting to access the `this` keyword without a class is an error.
Take for instance the Olive program below:
```
class names {
	init(name) {
		this.name = name;
	}
	
	change_name(new_name) {
		this.name = new_name;
	}
}

var a = names("James");
print "My name is " + a.name; // My name is James
a.name = "Mubarak";
print "My name is " + a.name; // My name is Mubarak
a.change_name("Olive");
print "My name is " + a.name; // My name is Olive
```
Access the fields, attributes or methods of a class using the `.` token and remove fields or attributes by simply calling the native `del_attr()` function like so `del_attr("instance", name);`.

```
del_attr("a", name);
print "My name is " + a.name; // error!
```
And for derived and base classes, here's a simple example:
```
class base_class {
  init(info) {
    print info;
  }
  
  print_info() {
  	print "Base class method.";
  }
}

class derived_class : base_class {
  init(info) {
    print info;
  }
  
  print_info() {
    base.print_info();
    print "Derived class method.";
  }
}

var a = base_class("I'm the based class."); // I'm the base class
a.print_info(); // Base class method

var b = derived_class("I'm the derived class."); // I'm the derived class
b.print_info(); // Derived class method
                // Base class method
```
As shown, derived classes can inherit the methods of their base classes. In the example, calling the `print_info()` method on the `derived_class` instance `b` goes on to further call the `print_info()` method of it's base class as defined in the derived class method. Note that derived classes inherit **only** their base class methods not their fields. This is because each base class instance could have fields initialized with different values over the course of execution. Attempting to inherit a class' fields begs the question then of which instance's fields to inherit. For what it's worth, trying that is an runtime error.
### Loops and Switch statements

Below is a `while` loop program written in Olive to print numbers starting from the number 1, skipping past numbers 2 and 4 and breaking at the number 20;

```
var a = 1;

while (a > 0) {
	if (a == 2) {
		a = a + 1;
		continue;
	}
	
	if (a == 4) {
		a = a + 1;
		continue;
	}
	
	if (a == 20) {
		print a;
		break;
	}
	
	print a;
	a = a + 1;
}
```
And a corresponding `for` loop program written in Olive to do the same;
```
for (var a = 1; a > 0; a = a + 1) {
	if (a == 2) {
		a = a + 1;
		continue;
	}
	
	if (a == 4) {
		a = a + 1;
		continue;
	}
	
	if (a == 20) {
		print a;
		break;
	}
	
	print a;
}
```
Olive supports statement encapsulation. Take for instance the Olive program below:
```
for (var a = 0; a < 10; a = a + 1) {
	switch(a) {
		case 1: {
			print a;
			break;
		}
		
		case 3: {
			print 3;
			break;
		}
		
		case 5: {
			continue;
		}
		
		default: {
			print "default";
		}
	}
  
  if (a > 8) break;
}
```
In the example above, a `switch` statement is enclosed by a `for` loop. Note that the `break` statements within the `switch` statement are tied to the `switch` statement and are therefore treated as such. The `contiue` statement, on the other hand, is tied to the enclosing `for` loop and is treated likewise.

Another example is a program with enclosed loops:

```
for (var a = 0; a < 10; a = a + 1) {
	var b = a;
	while (b > 0) {
		print b;
		b = b - 1;
    
    		if (b == 2) {
    			break;
    		}
  	}
}
```
In this example, the `break` statement within the `while` loop is tied to the `while` loop and doesn't link beyond it. This holds true for `continue` statements as well. To put it rather formally, a control statement is `tied` to the immediate loop or switch statement it is defined in. Note that the control statements; `break`, `continue`, are reserved keywords and cannot be used as identifiers or would result in a parsing error. Using them outside a loop statement (`break` and `continue` statements) or a `switch` statement (`break` statements) is a parsing error as well.

This simplicity in syntax would no doubt serve the novice programmer, simplifying the learning process.

## Features
Olive provides essential features like:
#### String concatenation

Using the '+' operator:
```
var a = "phew, Olive" + " works!";
````
The resulting concatenated string can then be printed or further concatenated with more strings:
```
print a; // phew, Olive works!
a = a + " Try it!";
print a; // phew, Olive works! Try it!
```
Or perhaps numbers or booleans:
```
a = a + 13;
print a; // phew, Olive works! Try it!13
```
#### String interpolation

Here's a simple Olive program using interpolated strings;
```
var name = "Ben";
var age = 8;
print "${name} is ${age + age} years old."; // Ben is 16 years old.
```
And if you're yet even crazier, there's support for nested interpolations as well;
```
print "Nested ${"interpolation?! Are you ${"mad?!"} Here goes"} crazy...";
```
Which prints, as you'd expect;
```
Nested interpolation?! Are you mad?! Here goes crazy...
```
#### C-style comments
I love C, it's a pain but, oh well. Olive allows C-style comments as well; both single-line and multi-line.
```
// DO NOT execute this
print "Execute this"; // Execute this
/* DO NOT execute this either
   I mean it*/
print "Execute this as well!";
```
#### One more thing. An REPL session

To start an REPL (Read-Execute-Print-Loop) session within the interpreter, navigate to the directory with your Olive executable (it's `Olive-bci` by default) and run this command within:

`$ ./olive`

![repl](https://github.com/wldfngrs/Olive/blob/main/Olive-bci/asset/repl.png)

You can now test out the syntax, quirks and perhaps force a few bugs (and I'll work on them) from within the REPL. Interestingly, Olive's REPL has 'persistence' allowing it to save the state of objects declared throughout the REPL session. Take the example below;

![persistence](https://github.com/wldfngrs/Olive/blob/main/Olive-bci/asset/persistence.png)

As shown above, the error from the undeclared variable 'intentional_error!' does not corrupt the state of the correctly declared variable 'a' and so is accessible from the 'print' statement. Hence 'persistence'.

Quit the REPL session with the `exit` command:

![exit](https://github.com/wldfngrs/Olive/blob/main/Olive-bci/asset/exit.png)

And that's it for REPLs. For a more standard program execution, you can write your first Olive test program and execute like so (don't forget to save with the .olv extension!):

```
$ ./olive [test_program.olv]
```
Whew.

## Features for the future
And there you have it. A brief introduction to Olive's syntax and VM. And yes, that's about all the particularly important features. Olive is a small language, for now, and I intend to improve it significantly as time goes on. This a "hobbyist's programming language" after all. Features coming soon to Olive include:
+ A lot more string manipulation functions

  I particularly enjoy messing with strings and so I'd work on native functions for string manipulation needs; from the pretty standard to the gnarly.
+ An improved function call routine

  Olive's function calls right now are not as fast as I'd like them to be. There's a lot of memory allocation sites borrowed from Lox that I don't particularly like. I'd switch that for "jump calls" that'd hopefully run faster on the Olive VM (bytecodes and all).
+ Arrays, arrays, arrays

  This is programming languages 101, I'm aware. I have a few ideas. I'd get on to it!

That's about it. I'd include more and update the features list as I commit more improved Olive versions. Programming languages certainly are complex beasts but I've tried to extensively comment any complex parts of the code as well as descriptive function names. If it's still a miss, send me an email: wldfngrs@gmail.com. And if you'd like to lend a hand with development, you're welcome. Dive in.


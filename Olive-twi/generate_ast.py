import sys

def defineAst(outputDir, baseName, Types):
	path = outputDir + "/" + baseName + ".cpp"
	with open(path, "a") as cppFile:
		cppFile.write("#include \"Token.h\"\n\n")
		
		defineVisitor(cppFile, baseName, Types)	
		
		cppFile.write("struct Expr {\n")
		cppFile.write("\tconst Expr left;\n")
		cppFile.write("\tconst Token oprtr;\n")
		cppFile.write("\tconst Expr right;\n\n")
		cppFile.write("\tvirtual void accept(visitorInterface& visitor);\n");
		cppFile.write("};\n\n");
		
		for element in Types:
			structName = element.split(":")[0].strip()
			fields = element.split(":")[1].strip()
			defineType(cppFile, baseName, structName, fields)
		cppFile.close()

def defineVisitor(cppFile, baseName, Types):
	cppFile.write("struct visitorInterface {\n")
	for Type in Types:
		typename = Type.split(":")[0].strip();
		cppFile.write("\tvirtual void visit" + typename + baseName + "(" + typename + " " + baseName.lower() + ") = 0;\n")
	cppFile.write("};\n\n")

def defineType(cppFile, baseName, structName, fieldList):
	cppFile.write("struct " + structName + " : " + baseName + " {\n")
	cppFile.write("\t" + structName + " (" + fieldList + ") {\n")
	
	fields = fieldList.split(", ")
	for field in fields:
		name = field.split(" ")[1]
		cppFile.write("\t\tthis." + name + " = " + name + ";\n")
		
	cppFile.write("\t}\n\n")
	
	for field in fields:
		cppFile.write("\tconst " + field + ";\n")
	cppFile.write("\n\tvirtual void accept(visitorInterface& visitor) {\n")
	cppFile.write("\t\tvisitor.visit" + structName + baseName + "(*this);\n")
	cppFile.write("\t}\n")
	cppFile.write("};\n\n")

if (len(sys.argv) != 2):
	print("Usage: generate_ast <output directory>")
	sys.exit()

outputDir = sys.argv[1]

ast = ["Binary	 : Expr left, Token oprtr, Expr right",
       "Grouping : Expr expression",
       "Literal  : Object value",
       "Unary	 : Token oprtr, Expr right"]
       
defineAst(outputDir, "Expr", ast)

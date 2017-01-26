#include "Parser.hpp"

extern "C" {
    int yylex();
    
    char * yytext;
    
    void * yy_scan_string(const char *);

    void yy_free_current_buffer(void);

    size_t get_linum(void);
}

namespace coralc {
    static inline void Error(const std::string & err) {
	// TODO: Line and column info + context!!!
	std::string errMsg = "Error [line " +
	    std::to_string(get_linum()) + "]:\n\t" + err + "\n";
	throw std::runtime_error(errMsg);
    }
    
    void Parser::Expect(const Token tok, const char * str) {
	this->NextToken();
	if (m_currentToken.id != tok) {
	    Error(str);
	}
    }
    
    Parser::Parser() : m_currentToken({Token::ENDOFFILE, {}}) {}

    std::deque<Parser::TokenInfo> Parser::ParseExprToRPN() {
	// Shunting Yard algorithm
	std::stack<TokenInfo> operatorStack;
	std::deque<TokenInfo> outputQueue;
	const auto precedence =
	    [](Token t) {
		switch (t) {
		case Token::MULTIPLY: return 3;
		case Token::DIVIDE: return 3;
		case Token::ADD: return 2;
		case Token::SUBTRACT: return 2;
		case Token::EQUALITY: return 1;
		default: Error("Unexpected token");
		}
	    };
	do {
	    switch (m_currentToken.id) {
	    case Token::EXPREND:
		while (!operatorStack.empty()) {
		    outputQueue.push_back(operatorStack.top());
		    operatorStack.pop();
		}
		std::cerr << std::endl;
		return outputQueue;

	    case Token::ASSIGN:
		Error("Assignment not allowed in rhs expression");

	    case Token::END:
	    case Token::ENDOFFILE:
		Error("Expected ;");

	    case Token::INTEGER:
	    case Token::FLOAT:
	    case Token::IDENT:
	    case Token::BOOLEAN:
		outputQueue.push_back(m_currentToken);
		break;
		
	    case Token::ADD:
	    case Token::SUBTRACT:
	    case Token::MULTIPLY:
	    case Token::DIVIDE:
	    case Token::EQUALITY:
		while (!operatorStack.empty() && precedence(operatorStack.top().id)
		       >= precedence(m_currentToken.id)) {
		    outputQueue.push_back(operatorStack.top());
		    operatorStack.pop();
		}
		operatorStack.push(m_currentToken);
		break;
	    }
	    this->NextToken();
	} while (true);
    }

    std::pair<ast::NodeRef, std::string>
    Parser::MakeExprSubTree(std::deque<Parser::TokenInfo> && exprQueueRPN) {
	// This function takes an RPN formatted expression and uses
	// it to construct a syntax subtree representing that expression.
	struct Value {
	    ast::NodeRef node;
	    std::string type;
	};
	auto OperandTypeError = [](const std::string & t1,
				   const std::string & t2) {
				    Error("Operand type mismatch: " + t1 + " and " + t2);
				};
	std::stack<Value> valueStack;
	auto GetValueStackTopTwo = [&valueStack, OperandTypeError]()
	    -> std::pair<Value, Value> {
	    auto rhs = std::move(valueStack.top());
	    valueStack.pop();
	    auto lhs = std::move(valueStack.top());
	    valueStack.pop();
	    if (lhs.type != rhs.type) {
		OperandTypeError(lhs.type, rhs.type);
	    }
	    return {std::move(lhs), std::move(rhs)};
	};
	std::string exprType = "";
	while (!exprQueueRPN.empty()) {
	    auto & curr = exprQueueRPN.front();
	    switch (curr.id) {
	    case Token::BOOLEAN:
		valueStack.push({
			ast::NodeRef(new ast::Boolean(curr.text == "true")),
			"bool"
		    });
		break;
		
	    case Token::INTEGER:
		valueStack.push({
			ast::NodeRef(new ast::Integer(std::stoi(curr.text))),
			"int"
		    });
		break;

	    case Token::FLOAT:
		valueStack.push({
			ast::NodeRef(new ast::Float(std::stof(curr.text))),
			"float"
		    });
		break;
		
	    case Token::IDENT:
		if (m_varTable.find(curr.text) == m_varTable.end()) {
		    Error("Attempt to reference nonexistent variable " + curr.text);
		}
		valueStack.push({
			ast::NodeRef(new ast::Ident(curr.text)),
			m_varTable[curr.text].type
		    });
		break;

	    case Token::EQUALITY: {
		auto operands = GetValueStackTopTwo();
		auto equalityOpRef =
		    std::make_unique<ast::EqualityOp>(operands.first.type,
						      std::move(operands.first.node),
						      std::move(operands.second.node));
		valueStack.push({ast::NodeRef(equalityOpRef.release()), "bool"});
	    } break;

	    case Token::ADD: {
		auto operands = GetValueStackTopTwo();
		if (operands.first.type == "bool" || operands.second.type == "bool") {
		    Error("Type bool cannot be used for arithmetic operations");
		}
		auto addOpRef =
		    std::make_unique<ast::AddOp>(operands.first.type,
						 std::move(operands.first.node),
						 std::move(operands.second.node));
		valueStack.push({ast::NodeRef(addOpRef.release()), operands.first.type});
	    } break;

	    case Token::SUBTRACT: {
		auto operands = GetValueStackTopTwo();
		if (operands.first.type == "bool" || operands.second.type == "bool") {
		    Error("Type bool cannot be used for arithmetic operations");
		}
		auto subOpRef =
		    std::make_unique<ast::SubOp>(operands.first.type,
						 std::move(operands.first.node),
						 std::move(operands.second.node));
		valueStack.push({ast::NodeRef(subOpRef.release()), operands.first.type});
	    } break;

	    case Token::MULTIPLY: {
		auto operands = GetValueStackTopTwo();
		auto multOpRef =
		    std::make_unique<ast::MultOp>(operands.first.type,
						  std::move(operands.first.node),
						  std::move(operands.second.node));
		valueStack.push({ast::NodeRef(multOpRef.release()), operands.first.type});
	    } break;

	    case Token::DIVIDE: {
		auto operands = GetValueStackTopTwo();
		auto divOpRef =
		    std::make_unique<ast::DivOp>(operands.first.type,
						 std::move(operands.first.node),
						 std::move(operands.second.node));
		valueStack.push({ast::NodeRef(divOpRef.release()), operands.first.type});
	    } break;
	    }
	    exprQueueRPN.pop_front();
	}
	if (valueStack.size() == 0) {
	    return {nullptr, "void"};
	} else if (valueStack.size() != 1) {
	    throw std::runtime_error("__Internal error: failed to build expression tree");
	}
	return {std::move(valueStack.top().node), valueStack.top().type};
    }

    ast::NodeRef Parser::ParseExpression() {
	auto exprQueueRPN = this->ParseExprToRPN();
	auto exprTreeInfo = this->MakeExprSubTree(std::move(exprQueueRPN));
	return ast::NodeRef(new ast::Expr(exprTreeInfo.second,
					  std::move(exprTreeInfo.first)));
    }
    
    ast::NodeRef Parser::ParseReturn() {
	this->NextToken();
	auto exprNode = this->ParseExpression();
	auto expr = dynamic_cast<ast::Expr *>(exprNode.get());
	assert(expr);
	if (m_currentFunction.returnType == "") {
	    m_currentFunction.returnType = expr->GetType();
	} else {
	    if (m_currentFunction.returnType != expr->GetType()) {
		const std::string errMsg = "Return type mismatch in function " +
		    m_currentFunction.name + ": " + expr->GetType() + " and " +
		    m_currentFunction.returnType;
		Error(errMsg);
	    }
	}
	if (expr->GetType() == "void") {
	    return ast::NodeRef(new ast::Return(ast::NodeRef(new ast::Void)));
	} else {
	    return ast::NodeRef(new ast::Return(std::move(exprNode)));
	}
    }
    
    ast::NodeRef Parser::ParseFor() {
	ast::NodeRef cmpLoopVar(nullptr);
	ast::NodeRef rangeStart(nullptr);
	ast::NodeRef rangeEnd(nullptr);
	this->Expect(Token::IDENT, "Expected identifier");
	std::string loopVarName = m_currentToken.text;
	ast::NodeRef declLoopVar(new ast::Ident(loopVarName));
	if (m_varTable.find(loopVarName) != m_varTable.end()) {
	     Error("Declaration of " + loopVarName + " would create a shadowing condition");
	}
	m_varTable[loopVarName].type = "int";
	m_varTable[loopVarName].isMutable = false;
	this->Expect(Token::IN, "Expected in");
	this->NextToken();
	switch (m_currentToken.id) {
	case Token::INTEGER:
	    rangeStart = ast::NodeRef(new ast::Integer(std::stoi(m_currentToken.text)));
	    break;

	case Token::IDENT:
	    rangeStart = ast::NodeRef(new ast::Ident(m_currentToken.text));
	    break;

	default:
	    Error("Expected integer or identifier");
	}
	this->Expect(Token::RANGE, "Expected ..");
	this->NextToken();
	switch (m_currentToken.id) {
	case Token::INTEGER:
	    rangeEnd = ast::NodeRef(new ast::Integer(std::stoi(m_currentToken.text)));
	    break;

	case Token::IDENT:
	    rangeEnd = ast::NodeRef(new ast::Ident(m_currentToken.text));
	    break;

	default:
	    Error("Expected integer or identifier");
	}
        auto decl = std::make_unique<ast::DeclIntVar>(std::move(declLoopVar),
						      std::move(rangeStart));
	this->Expect(Token::DO, "Expected do");
	this->NextToken();
	auto scope = this->ParseScope();
	m_varTable.erase(loopVarName);
	return ast::NodeRef(new ast::ForLoop(std::move(decl),
					     std::move(rangeEnd),
					     std::move(scope)));
    }

    ast::NodeRef Parser::ParseFunctionDef() {
	m_currentFunction.returnType = "";
	this->Expect(Token::IDENT, "Expected identifier");
	std::string fname = m_currentToken.text;
	m_currentFunction.name = fname;
	this->Expect(Token::LPRN, "Expected (");
	// TODO: function parameters... !!!
	this->Expect(Token::RPRN, "Expected )");
	this->NextToken();
	auto scope = this->ParseScope();
	const bool hasExplicitReturnStatement = m_currentFunction.returnType != "";
	if (hasExplicitReturnStatement) {
	    if (!scope->GetChildren().empty()) {
	        auto ret =
		    dynamic_cast<ast::Return *>(scope->GetChildren().back().get());
		if (!ret) {
		    if (m_currentFunction.returnType == "void") {
			auto implicitVoidRet =
			    ast::NodeRef(new ast::Return(ast::NodeRef(new ast::Void)));
			scope->AddChild(std::move(implicitVoidRet));
		    } else {
			Error("Missing return in non-void function");
		    }
		}
	    }
	} else /* !hasExplicitReturnStatement */ {
	    m_currentFunction.returnType = "void";
	    scope->AddChild(ast::NodeRef(new ast::Return(ast::NodeRef(new ast::Void))));
	}
	return ast::NodeRef(new ast::Function(std::move(scope),
					      fname, m_currentFunction.returnType));
    }

    ast::NodeRef Parser::ParseTopLevelScope() {
	auto topLevel = std::make_unique<ast::GlobalScope>();
	do {
	    switch (m_currentToken.id) {
	    case Token::DEF:
		topLevel->AddChild(this->ParseFunctionDef());
		break;

	    case Token::VAR:
	    case Token::MUT:
		Error("Global variables are not allowed");

	    case Token::END:
		Error("Unexpected end");
	    }
	    this->NextToken();
	} while (m_currentToken.id != Token::ENDOFFILE);
	return ast::NodeRef(topLevel.release());
    }

    ast::NodeRef Parser::ParseDeclVar(const bool mut) {
	this->Expect(Token::IDENT, "Expected identifier after var");
	std::string identName = m_currentToken.text;
	auto ident = ast::NodeRef(new ast::Ident(identName));
	if (m_varTable.find(identName) != m_varTable.end()) {
	    if (m_localVars->find(identName) != m_localVars->end()) {
		Error("Re-declaration of " + identName);
	    } else {
		Error("Declaration of " + identName + " would create a shadowing condition");
	    }
	}
	m_localVars->insert(identName);
	this->Expect(Token::ASSIGN, "Expected =");
	this->NextToken();
	auto expr = this->ParseExpression();
	auto & exprType = dynamic_cast<ast::Expr *>(expr.get())->GetType();
	if (exprType == "int") {
	    m_varTable[identName].type = "int";
	    m_varTable[identName].isMutable = mut;
	    return ast::NodeRef(new ast::DeclIntVar(std::move(ident),
						    std::move(expr)));
	} else if (exprType == "float") {
	    m_varTable[identName].type = "float";
	    m_varTable[identName].isMutable = mut;
	    return ast::NodeRef(new ast::DeclFloatVar(std::move(ident),
						      std::move(expr)));
	} else if (exprType == "bool") {
	    m_varTable[identName].type = "bool";
	    m_varTable[identName].isMutable = mut;
	    return ast::NodeRef(new ast::DeclBooleanVar(std::move(ident),
							std::move(expr)));
	} else if (exprType == "void") {
	    Error("Attempt to bind void to an l-value");
	}
    }
    
    ast::ScopeRef Parser::ParseScope() {
	std::set<std::string> localVars;
	std::set<std::string> * parentScopeVars;
	parentScopeVars = m_localVars;
	m_localVars = &localVars;
	bool unreachable = false;
	ast::ScopeRef scope(new ast::Scope);
	do {
	    if (!unreachable) {
		switch (m_currentToken.id) {
		case Token::FOR: {
		    scope->AddChild(this->ParseFor());
		} break;
		    
		case Token::VAR:
		    scope->AddChild(this->ParseDeclVar(false));
		    break;
		    
		case Token::MUT:
		    this->Expect(Token::VAR, "Expected var");
		    scope->AddChild(this->ParseDeclVar(false));
		    break;
		    
		case Token::END:
		    goto CLEANUPSCOPE;

		case Token::ENDOFFILE:
		    Error("Expected end");
		    
		case Token::RETURN:
		    // After seeing a return, no other code in the
		    // current scope can be executed. Perhaps send
		    // a warning to the user?
		    unreachable = true;
		    {
			auto ret = this->ParseReturn();
			scope->AddChild(std::move(ret));
		    }
		    break;

		default: {
		    Error(m_currentToken.text);
		} break;
		}
	    } else {
		if (m_currentToken.id == Token::END) {
		    goto CLEANUPSCOPE;
		}
	    }
	    this->NextToken();
	} while (true);
    CLEANUPSCOPE:
	for (auto & element : localVars) {
	    m_varTable.erase(element);
	}
	m_localVars = parentScopeVars;
	return scope;
    }
    
    ast::NodeRef Parser::Parse(const std::string & sourceFile) {
	yy_scan_string(sourceFile.c_str());
	this->NextToken();
	ast::NodeRef astRoot(nullptr);
	try {
	    astRoot = this->ParseTopLevelScope();
	} catch (const std::exception & ex) {
	    std::cerr << ex.what() << std::endl;
	    throw std::runtime_error("Compilation failed");
	}
	yy_free_current_buffer();
	return astRoot;
    }

    void Parser::NextToken() {
	m_currentToken = TokenInfo{static_cast<Token>(yylex()), std::string(yytext)};
    }
}

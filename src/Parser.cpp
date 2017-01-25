#include "Parser.hpp"

extern "C" {
    int yylex();
    
    char * yytext;
    
    void * yy_scan_string(const char *);

    void yy_free_current_buffer(void);
}

namespace coralc {
    static inline void Error(const std::string & err) {
	// TODO: Line and column info + context!!!
	throw std::runtime_error("Error: " + err);
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
	const auto prec =
	    [](Token t) {
		switch (t) {
		case Token::MULTIPLY: return 3;
		case Token::DIVIDE: return 3;
		case Token::ADD: return 2;
		case Token::SUBTRACT: return 2;
		}
	    };
	do {
	    switch (m_currentToken.id) {
	    case Token::EXPREND:
		while (!operatorStack.empty()) {
		    outputQueue.push_back(operatorStack.top());
		    operatorStack.pop();
		}
		return outputQueue;

	    case Token::ASSIGN:
		Error("assignment not allowed here");

	    case Token::END:
	    case Token::ENDOFFILE:
		Error("expected ;");

	    case Token::INTEGER:
	    case Token::IDENT:
		outputQueue.push_back(m_currentToken);
		break;
		
	    case Token::ADD:
	    case Token::SUBTRACT:
	    case Token::MULTIPLY:
	    case Token::DIVIDE:
		while (!operatorStack.empty() && prec(operatorStack.top().id)
		       >= prec(m_currentToken.id)) {
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
	std::stack<ast::NodeRef> valueStack;
	std::string exprType = "";
	while (!exprQueueRPN.empty()) {
	    auto & curr = exprQueueRPN.front();
	    switch (curr.id) {
	    case Token::INTEGER:
		if (exprType == "") {
		    exprType = "int";
		} else if (exprType != "int") {
		    Error("type misatch in expression, int and " + exprType);
		}
		valueStack.push(ast::NodeRef(new ast::Integer(std::stoi(curr.text))));
		break;
		
	    case Token::IDENT:
		if (m_varTable.find(curr.text) == m_varTable.end()) {
		    Error("attempt to reference nonexistent variable " + curr.text);
		}
		if (exprType == "") {
		    exprType = m_varTable[curr.text].type;
		} else if (exprType != m_varTable[curr.text].type) {
		    Error("type mismatch in expression, " +
			  m_varTable[curr.text].type + " and" + exprType);
		}
		valueStack.push(ast::NodeRef(new ast::Ident(curr.text)));
		break;

	    case Token::ADD: {
		auto rhs = std::move(valueStack.top());
		valueStack.pop();
		auto lhs = std::move(valueStack.top());
		valueStack.pop();
		auto addOpRef =
		    std::make_unique<ast::AddOp>(std::move(lhs), std::move(rhs));
		valueStack.push(ast::NodeRef(addOpRef.release()));
	    } break;

	    case Token::SUBTRACT: {
		auto rhs = std::move(valueStack.top());
		valueStack.pop();
		auto lhs = std::move(valueStack.top());
		valueStack.pop();
		auto subOpRef =
		    std::make_unique<ast::SubOp>(std::move(lhs), std::move(rhs));
		valueStack.push(ast::NodeRef(subOpRef.release()));
	    } break;

	    case Token::MULTIPLY: {
		auto rhs = std::move(valueStack.top());
		valueStack.pop();
		auto lhs = std::move(valueStack.top());
		valueStack.pop();
		auto multOpRef =
		    std::make_unique<ast::MultOp>(std::move(lhs), std::move(rhs));
		valueStack.push(ast::NodeRef(multOpRef.release()));
	    } break;

	    case Token::DIVIDE: {
		auto rhs = std::move(valueStack.top());
		valueStack.pop();
		auto lhs = std::move(valueStack.top());
		valueStack.pop();
		auto divOpRef =
		    std::make_unique<ast::DivOp>(std::move(lhs), std::move(rhs));
		valueStack.push(ast::NodeRef(divOpRef.release()));
	    } break;
	    }
	    exprQueueRPN.pop_front();
	}
	if (valueStack.size() == 0) {
	    return {nullptr, "void"};
	} else if (valueStack.size() != 1) {
	    throw std::runtime_error("__Internal error: failed to build expression tree");
	}
	return {std::move(valueStack.top()), exprType};
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
		const std::string errMsg = "return type mismatch in function " +
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
	this->Expect(Token::IDENT, "expected identifier");
	std::string loopVarName = m_currentToken.text;
	ast::NodeRef declLoopVar(new ast::Ident(loopVarName));
	if (m_varTable.find(loopVarName) != m_varTable.end()) {
	    Error("declaration of " + loopVarName + " would shadow existing variable");
	}
	m_varTable[loopVarName].type = "int";
	this->Expect(Token::IN, "expected in");
	this->NextToken();
	switch (m_currentToken.id) {
	case Token::INTEGER:
	    rangeStart = ast::NodeRef(new ast::Integer(std::stoi(m_currentToken.text)));
	    break;

	case Token::IDENT:
	    rangeStart = ast::NodeRef(new ast::Ident(m_currentToken.text));
	    break;

	default:
	    Error("expected integer or identifier");
	}
	this->Expect(Token::RANGE, "expected ..");
	this->NextToken();
	switch (m_currentToken.id) {
	case Token::INTEGER:
	    rangeEnd = ast::NodeRef(new ast::Integer(std::stoi(m_currentToken.text)));
	    break;

	case Token::IDENT:
	    rangeEnd = ast::NodeRef(new ast::Ident(m_currentToken.text));
	    break;

	default:
	    Error("expected integer or identifier");
	}
        auto decl = std::make_unique<ast::DeclImmutIntVar>(std::move(declLoopVar),
							   std::move(rangeStart));
	this->Expect(Token::DO, "expected do");
	this->NextToken();
	auto scope = this->ParseScope();
	m_varTable.erase(loopVarName);
	return ast::NodeRef(new ast::ForLoop(std::move(decl),
					     std::move(rangeEnd),
					     std::move(scope)));
    }

    ast::NodeRef Parser::ParseFunctionDef() {
	m_currentFunction.returnType = "";
	this->Expect(Token::IDENT, "expected identifier");
	std::string fname = m_currentToken.text;
	m_currentFunction.name = fname;
	this->Expect(Token::LPRN, "expected (");
	// TODO: function parameters... !!!
	this->Expect(Token::RPRN, "expected )");
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
			Error("missing return in non-void function");
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

	    case Token::END:
		Error("unexpected end");
	    }
	    this->NextToken();
	} while (m_currentToken.id != Token::ENDOFFILE);
	return ast::NodeRef(topLevel.release());
    }

    ast::NodeRef Parser::ParseDeclVar() {
	this->Expect(Token::IDENT, "expected identifier after var");
	std::string identName = m_currentToken.text;
	auto ident = ast::NodeRef(new ast::Ident(identName));
	m_localVars->insert(identName);
	if (m_varTable.find(identName) != m_varTable.end()) {
	    Error("re-declaration of " + identName);
	}
	this->Expect(Token::ASSIGN, "expected =");
	this->NextToken();
	auto expr = this->ParseExpression();
	auto & exprType = dynamic_cast<ast::Expr *>(expr.get())->GetType();
	if (exprType == "int") {
	    m_varTable[identName].type = "int";
	    return ast::NodeRef(new ast::DeclImmutIntVar(std::move(ident),
							 std::move(expr)));
	} else {
	    Error("non-int types aren't supported yet!");
	}
    }

    ast::NodeRef Parser::ParseDeclMutVar() {
	this->Expect(Token::VAR, "expected var after mut");
	this->Expect(Token::IDENT, "expected identifier after var");
	std::string identName = m_currentToken.text;
	if (m_varTable.find(identName) != m_varTable.end()) {
	    Error("declaration of " + identName + " would shadow existing variable");
	}
	this->Expect(Token::ASSIGN, "expected =");
	this->NextToken();
	auto expr = this->ParseExpression();
	Error("parser incomplete for mutable variables...");
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
		    scope->AddChild(this->ParseDeclVar());
		    break;
		    
		case Token::MUT:
		    this->ParseDeclMutVar();
		    break;
		    
		case Token::END:
		    goto CLEANUPSCOPE;

		case Token::ENDOFFILE:
		    Error("expected end");
		    
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

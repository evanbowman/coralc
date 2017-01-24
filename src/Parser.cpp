#include "Parser.hpp"

extern "C" {
    int yylex();
    
    char * yytext;
    
    void * yy_scan_string(const char *);

    void yy_free_current_buffer(void);
}

namespace coralc {
    Parser::Parser() : m_currentToken({Token::ENDOFFILE, {}}) {}

    static inline void Error(const std::string & err) {
	// TODO: Line and column info + context!!!
	throw std::runtime_error("Error: " + err);
    }

    ast::NodeRef Parser::ParseReturn() {
	ast::NodeRef ret(new ast::Return(std::move(ast::NodeRef(new ast::Void))));
	// All returns are void at the moment...
	// until I can come up with an actually well thought out
	// type system...
	return ret;
    }
    
    ast::NodeRef Parser::ParseFor() {
	this->NextToken();
	ast::NodeRef declLoopVar(nullptr);
	ast::NodeRef cmpLoopVar(nullptr);
	ast::NodeRef rangeStart(nullptr);
	ast::NodeRef rangeEnd(nullptr);
	if (m_currentToken.id == Token::IDENT) {
	    declLoopVar = ast::NodeRef(new ast::Ident(m_currentToken.text));
	} else Error("expected identifier");
	this->NextToken();
	if (m_currentToken.id != Token::IN) {
	    Error("expected in");
	}
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
	this->NextToken();
	if (m_currentToken.id != Token::RANGE) {
	    Error("expected ..");
	}
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
	this->NextToken();
	if (m_currentToken.id != Token::DO) {
	    Error("expected do");
	}
	this->NextToken();
	auto scope = this->ParseScope();
	return ast::NodeRef(new ast::ForLoop(std::move(decl),
					     std::move(rangeEnd),
					     std::move(scope)));
    }

    ast::NodeRef Parser::ParseFunctionDef() {
	this->NextToken();
	if (m_currentToken.id != Token::IDENT) {
	    throw std::runtime_error("expected identifier");
	}
	std::string fname = m_currentToken.text;
	m_currentFunction.name = fname;
	this->NextToken();
	if (m_currentToken.id != Token::LPRN) {
	    throw std::runtime_error("expected (");
	}
	// TODO: function parameters... !!!
	this->NextToken();
	if (m_currentToken.id != Token::RPRN) {
	    throw std::runtime_error("expected )");
	}
	this->NextToken();
	auto scope = this->ParseScope();
	return ast::NodeRef(new ast::Function(std::move(scope),
					      fname, m_currentFunction.name));
    }

    ast::NodeRef Parser::ParseTopLevelScope() {
	auto topLevel = std::make_unique<ast::GlobalScope>();
	do {
	    switch (m_currentToken.id) {
	    case Token::DEF:
		topLevel->AddChild(this->ParseFunctionDef());
		break;

	    }
	    this->NextToken();
	} while (m_currentToken.id != Token::ENDOFFILE);
	return ast::NodeRef(topLevel.release());
    }
    
    ast::ScopeRef Parser::ParseScope() {
	bool unreachable = false;
	ast::ScopeRef scope(new ast::Scope);
	do {
	    if (!unreachable) {
		switch (m_currentToken.id) {
		case Token::FOR: {
		    scope->AddChild(this->ParseFor());
		} break;
		    
		case Token::VAR:
		    // TODO...
		    break;
		    
		case Token::MUT:
		    // TODO...
		    break;
		    
		case Token::END:
		    return scope;
		    
		case Token::RETURN:
		    // After seeing a return, no other code in the
		    // current scope can be executed. Perhaps send
		    // a warning to the user?
		    unreachable = true;
		    scope->AddChild(this->ParseReturn());
		    break;
		}
	    } else {
		if (m_currentToken.id == Token::END) {
		    return scope;
		}
	    }
	    this->NextToken();
	} while (m_currentToken.id != Token::ENDOFFILE);
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

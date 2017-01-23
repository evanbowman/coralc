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
        auto decl = std::make_unique<ast::DeclImmutVar>(std::move(declLoopVar),
							std::move(rangeStart));
	this->NextToken();
	if (m_currentToken.id != Token::DO) {
	    Error("expected do");
	}
	this->NextToken();
	auto block = this->ParseBlock();
	auto forLoop = std::make_unique<ast::ForLoop>(std::move(decl),
						      std::move(rangeEnd),
						      std::move(block));
	return ast::NodeRef(forLoop.release());
    }

    ast::NodeRef Parser::ParseBlock() {
	auto block = std::make_unique<ast::Block>();
	do {
	    switch (m_currentToken.id) {
	    case Token::FOR:
		block->AddChild(this->ParseFor());
		break;

	    case Token::VAR:
		// TODO...
		break;

	    case Token::MUT:
		// TODO...
		break;

	    case Token::END:
		return ast::NodeRef(block.release());

	    default:
		// IMPORTANT: remove this branch target
		break;
	    }
	    this->NextToken();
	} while (m_currentToken.id != Token::ENDOFFILE);
	return ast::NodeRef(block.release());
    }
    
    ast::NodeRef Parser::Parse(const std::string & sourceFile) {
	yy_scan_string(sourceFile.c_str());
	this->NextToken();
	ast::NodeRef astRoot(nullptr);
	try {
	    astRoot = this->ParseBlock();
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

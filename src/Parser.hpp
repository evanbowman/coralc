#pragma once

#include <string>
#include <utility>
#include <iostream>
#include <vector>
#include <memory>
#include <ostream>
#include <functional>
#include <array>
#include "ast.hpp"

namespace coralc {
    class Parser {
    public:
	Parser();
	ast::NodeRef Parse(const std::string &);
    
    private:
	enum class Token {
	    ENDOFFILE,
	    FOR,
	    IN,
	    DO,
	    RANGE,
	    END,
	    VAR,
	    DEF,
	    ASSIGN,
	    COMMA,
	    LBRACE,
	    RBRACE,
	    ADD,
	    SUBTRACT,
	    MULTIPLY,
	    DIVIDE,
	    RETURN,
	    MUT,
	    HASH,
	    LPRN,
	    RPRN,
	    STRING,
	    INTEGER,
	    IDENT
	};
	struct TokenInfo {
	    Token id;
	    std::string text;
	};
	struct FunctionInfo {
	    std::string name;
	    std::string returnType;
	};
	ast::NodeRef ParseTopLevelScope();
	ast::NodeRef ParseFunctionDef();
	ast::ScopeRef ParseScope();
	ast::NodeRef ParseReturn();
	ast::NodeRef ParseFor();
	void NextToken();
	TokenInfo m_currentToken;
	FunctionInfo m_currentFunction;
    };
}

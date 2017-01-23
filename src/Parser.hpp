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
	    ASSIGN,
	    COMMA,
	    LBRACE,
	    RBRACE,
	    ADD,
	    SUBTRACT,
	    MULTIPLY,
	    DIVIDE,
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
	ast::NodeRef ParseBlock();
	ast::NodeRef ParseFor();
	void NextToken();
	TokenInfo m_currentToken;
    };
}

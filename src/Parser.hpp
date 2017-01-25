#pragma once

#include<cassert>
#include <string>
#include <utility>
#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <ostream>
#include <functional>
#include <set>
#include <array>
#include <exception>
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
	    EXPREND,
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
	    TRUE,
	    FALSE,
	    RETURN,
	    MUT,
	    HASH,
	    LPRN,
	    RPRN,
	    STRING,
	    FLOAT,
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
	std::pair<ast::NodeRef, std::string> MakeExprSubTree(std::deque<TokenInfo> &&);
	std::deque<TokenInfo> ParseExprToRPN();
	ast::NodeRef ParseDeclVar(const bool);
	ast::NodeRef ParseExpression();
	ast::NodeRef ParseTopLevelScope();
	ast::NodeRef ParseFunctionDef();
	ast::ScopeRef ParseScope();
	ast::NodeRef ParseReturn();
	ast::NodeRef ParseFor();
	void NextToken();
	void Expect(const Token, const char *);
	TokenInfo m_currentToken;
	FunctionInfo m_currentFunction;
	// Types are represented within the compiler as strings. I'm
	// hoping that this will make lookups easier later for user
	// defined types (classes).
	struct VarInfo {
	    std::string type;
	    bool isMutable;
	};
	std::map<std::string, VarInfo> m_varTable;
	std::set<std::string> * m_localVars;
    };
}

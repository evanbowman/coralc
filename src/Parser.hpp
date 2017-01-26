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
	    EQUALITY,
	    INEQUALITY,
	    BOOLEAN,
	    RETURN,
	    MUT,
	    HASH,
	    LPRN,
	    RPRN,
	    AND,
	    OR,
	    STRING,
	    FLOAT,
	    INTEGER,
	    MODULE,
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
	void Error(const std::string &);
	template <Parser::Token Exprend>
	std::deque<Parser::TokenInfo> ParseExprToRPN() {
	    // Shunting Yard algorithm
	    std::stack<TokenInfo> operatorStack;
	    std::deque<TokenInfo> outputQueue;
	    const auto precedence =
		[this](Token t) {
		    switch (t) {
		    case Token::MULTIPLY: return 4;
		    case Token::DIVIDE: return 4;
		    case Token::ADD: return 3;
		    case Token::SUBTRACT: return 3;
		    case Token::EQUALITY: return 2;
		    case Token::INEQUALITY: return 2;
		    case Token::AND: return 1;
		    case Token::OR: return 1;
			// Contents within paretheses are executed first,
			// but the parentheses themselves have a low
			// precedence in the context of this algorithm.
		    case Token::LPRN:
		    case Token::RPRN: return 0;
		    default: this->Error("Unexpected token"); return 0;
		    }
		};
	    do {
		switch (m_currentToken.id) {
		case Exprend:
		    while (!operatorStack.empty()) {
			if (operatorStack.top().id == Token::LPRN ||
			    operatorStack.top().id == Token::RPRN) {
			    this->Error("Mis-matched parentheses");
			}
			outputQueue.push_back(operatorStack.top());
			operatorStack.pop();
		    }
		    return outputQueue;

		case Token::ASSIGN:
		    Error("Assignment not allowed in rhs expression");
		    break;
		    
		case Token::END:
		case Token::ENDOFFILE:
		    Error("Expected ;");
		    break;

		case Token::LPRN:
		    operatorStack.push(m_currentToken);
		    break;

		case Token::RPRN:
		    while (!operatorStack.empty()) {
			if (operatorStack.top().id == Token::LPRN) {
			    operatorStack.pop();
			    break;
			} else {
			    outputQueue.push_back(operatorStack.top());
			    operatorStack.pop();
			}
			if (operatorStack.empty()) {
			    this->Error("Mis-matched parentheses");
			}
		    }
		    break;
		    
		case Token::INTEGER:
		case Token::FLOAT:
		case Token::IDENT:
		case Token::BOOLEAN:
		    outputQueue.push_back(m_currentToken);
		    break;

		case Token::OR:
		case Token::AND:
		case Token::ADD:
		case Token::DIVIDE:
		case Token::SUBTRACT:
		case Token::MULTIPLY:
		case Token::EQUALITY:
		case Token::INEQUALITY:
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
        MakeExprSubTree(std::deque<Parser::TokenInfo> &&);
	ast::NodeRef ParseDeclVar(const bool);
	template <Token Exprend>
	ast::NodeRef ParseExpression() {
	    auto exprQueueRPN = this->ParseExprToRPN<Exprend>();
	    auto exprTreeInfo = MakeExprSubTree(std::move(exprQueueRPN));
	    return ast::NodeRef(new ast::Expr(exprTreeInfo.second,
					      std::move(exprTreeInfo.first)));
	}
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
	// defined types (classes). Maybe type could instead be a UID
	// into a type table of strings, to save on memory.
	struct VarInfo {
	    std::string type;
	    bool isMutable;
	};
	std::map<std::string, VarInfo> m_varTable;
	std::set<std::string> * m_localVars;
    };
}

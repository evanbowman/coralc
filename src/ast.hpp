#pragma once

#include <iostream>
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/ReaderWriter.h"

namespace coralc {
    struct FunctionInfo {
	llvm::AllocaInst * exitValue = nullptr;
	llvm::BasicBlock * exitPoint = nullptr;
    };
    struct LLVMState {
	llvm::LLVMContext context;
	llvm::IRBuilder<> builder;
	std::unique_ptr<llvm::Module> modRef;
	std::stack<llvm::BasicBlock *> stack;
	FunctionInfo currentFnInfo;
	std::map<std::string, llvm::AllocaInst *> vars;
	LLVMState() : builder(context),
		      modRef(std::make_unique<llvm::Module>("top", context)) {}
    };
    
    namespace ast {
	class Node;
	
	using NodeRef = std::unique_ptr<Node>;
	
	class Node {
	public:
	    virtual llvm::Value * CodeGen(LLVMState &) = 0;
	    virtual ~Node() {}
	};
	
	class Scope : public Node {
	    std::vector<NodeRef> m_children;
	public:
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	    void AddChild(NodeRef);
	    const std::vector<NodeRef> & GetChildren() const {
		return m_children;
	    }
	};

	struct GlobalScope : public Scope {
	    virtual llvm::Value * CodeGen(LLVMState & state) override {
		for (auto & child : this->GetChildren()) {
		    child->CodeGen(state);
		}
		return nullptr;
	    }
	};

	using ScopeRef = std::unique_ptr<Scope>;
	
	class ScopeProvider {
	    ScopeRef m_scope;
	public:
	    ScopeProvider(ScopeRef scope) : m_scope(std::move(scope)) {}
	    Scope & GetScope() {
		return *m_scope;
	    }
	};

	class Expr : public Node {
	    std::string m_type;
	    NodeRef m_exprSubTree;
	public:
	    Expr(const std::string & type, NodeRef tree) :
		m_type(type), m_exprSubTree(std::move(tree)) {}
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	    const std::string & GetType() const {
		return m_type;
	    }
	};

	class Return : public Node {
	    friend class Scope;
	    NodeRef m_value;
	public:
	    Return(NodeRef);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	class BinOp : public Node {
	protected:
	    std::string m_resultType;
	    NodeRef m_lhs, m_rhs;
	public:
	    BinOp(const std::string & type, NodeRef lhs, NodeRef rhs) :
		m_resultType(type), m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {}
	};

	struct MultOp : public BinOp {
	    MultOp(const std::string & type, NodeRef lhs, NodeRef rhs) :
		BinOp(type, std::move(lhs), std::move(rhs)) {}
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	struct DivOp : public BinOp {
	    DivOp(const std::string & type, NodeRef lhs, NodeRef rhs) :
		BinOp(type, std::move(lhs), std::move(rhs)) {}
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	struct AddOp : public BinOp {
	    AddOp(const std::string & type, NodeRef lhs, NodeRef rhs) :
		BinOp(type, std::move(lhs), std::move(rhs)) {}
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	struct SubOp : public BinOp {
	    SubOp(const std::string & type, NodeRef lhs, NodeRef rhs) :
		BinOp(type, std::move(lhs), std::move(rhs)) {}
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	struct EqualityOp : public BinOp {
	    EqualityOp(const std::string & type, NodeRef lhs, NodeRef rhs) :
		BinOp(type, std::move(lhs), std::move(rhs)) {}
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	class Function : public Node, public ScopeProvider {
	    std::string m_name;
	    std::string m_returnType;
	public:
	    Function(ScopeRef, const std::string &, const std::string &);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};
	
	class ForLoop : public Node, public ScopeProvider {
	    std::string m_varName;
	    NodeRef m_decl, m_end;
	public:
	    const std::string & GetIdentName() const;
	    ForLoop(NodeRef, NodeRef, ScopeRef);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	class Ident : public Node {
	    std::string m_name;
	public:
	    Ident(const std::string &);
	    const std::string & GetName() const;
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};
	
	class DeclVar : public Node {
	protected:
	    NodeRef m_ident, m_value;
	public:
	    const std::string & GetIdentName() const {
		return dynamic_cast<Ident &>(*m_ident).GetName();
	    }
	    DeclVar(NodeRef ident, NodeRef value);
	};
	
        struct DeclIntVar : public DeclVar {
	    DeclIntVar(NodeRef, NodeRef);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	struct DeclFloatVar : public DeclVar {
	    DeclFloatVar(NodeRef, NodeRef);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	struct DeclBooleanVar : public DeclVar {
	    DeclBooleanVar(NodeRef, NodeRef);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

        struct Void : public Node {
	public:
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	class Float : public Node {
	    float m_value;
	public:
	    Float(const float);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	class Boolean : public Node {
	    bool m_value;
	public:
	    Boolean(const bool);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};
	
	class Integer : public Node {
	    int m_value;
	public:
	    Integer(const int);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};	
    }
}

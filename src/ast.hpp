#pragma once

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
    struct VarInfo {
	llvm::AllocaInst * value;
	bool isMutable;
    };
    
    struct LLVMState {
	llvm::LLVMContext context;
	llvm::IRBuilder<> builder;
	std::unique_ptr<llvm::Module> modRef;
	std::stack<llvm::BasicBlock *> stack;
        llvm::BasicBlock * fnExitPoint = nullptr;
	std::map<std::string, VarInfo> vars;
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
	};

	class GlobalScope : public Scope {};

	using ScopeRef = std::unique_ptr<Scope>;
	
	class ScopeProvider {
	    ScopeRef m_scope;
	public:
	    ScopeProvider(ScopeRef scope) : m_scope(std::move(scope)) {}
	    Scope & GetScope() {
		return *m_scope;
	    }
	};

	class Return : public Node {
	    friend class Scope;
	    NodeRef m_value;
	public:
	    Return(NodeRef);
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

	class DeclImmutIntVar : public Node {
	    NodeRef m_ident, m_value;
	public:
	    const std::string & GetIdentName() const; 
	    DeclImmutIntVar(NodeRef, NodeRef);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

        struct Void : public Node {
	public:
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};
	
	class Integer : public Node {
	    int m_value;
	public:
	    Integer(const int);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	class Ident : public Node {
	    std::string m_name;
	public:
	    Ident(const std::string &);
	    const std::string & GetName() const;
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};
    }
}

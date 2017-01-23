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
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/ReaderWriter.h"

namespace coralc {
    struct VarInfo {
	llvm::Value * value;
	bool isMutable;
    };
    
    struct LLVMState {
	llvm::LLVMContext context;
	llvm::IRBuilder<> builder;
	std::unique_ptr<llvm::Module> modRef;
        std::map<std::string, VarInfo> vars;
	LLVMState() : builder(context),
		      modRef(std::make_unique<llvm::Module>("top", context)) {
	    auto funcType = llvm::FunctionType::get(builder.getVoidTy(), false);
	    auto mainFunc = 
		llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", modRef.get());
	    auto entry = llvm::BasicBlock::Create(context, "entrypoint", mainFunc);
	    builder.SetInsertPoint(entry);
	}
	~LLVMState() {
	    builder.CreateRetVoid();
	    llvm::WriteBitcodeToFile(modRef.get(), llvm::outs());
	}
    };
    
    namespace ast {
	class Node;
	
	using NodeRef = std::unique_ptr<Node>;
	
	class Node {
	public:
	    virtual llvm::Value * CodeGen(LLVMState &) = 0;
	    virtual ~Node() {}
	};

	class Block : public Node {
	    std::vector<NodeRef> m_children;
	public:
	    void AddChild(NodeRef);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	class ForLoop : public Node {
	    std::string m_varName;
	    NodeRef m_decl, m_end, m_block;
	public:
	    const std::string & GetIdentName() const;
	    ForLoop(NodeRef, NodeRef, NodeRef);
	    virtual llvm::Value * CodeGen(LLVMState &) override;
	};

	class DeclImmutVar : public Node {
	    NodeRef m_ident, m_value;
	public:
	    const std::string & GetIdentName() const; 
	    DeclImmutVar(NodeRef, NodeRef);
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

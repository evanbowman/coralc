#include "ast.hpp"

#include <iostream>

namespace coralc {
    namespace ast {
	void Scope::AddChild(NodeRef child) {
	    m_children.push_back(std::move(child));
	}

	Ident::Ident(const std::string & name) : m_name(name) {}

	Function::Function(ScopeRef scope, const std::string & name,
			   const std::string & returnType) :
	    ScopeProvider(std::move(scope)), m_name(name), m_returnType(returnType) {}
	
	const std::string & Ident::GetName() const {
	    return m_name;
	}

	Return::Return(NodeRef value) : m_value(std::move(value)) {}
	
	Integer::Integer(const int value) : m_value(value) {}

	ForLoop::ForLoop(NodeRef decl, NodeRef end, ScopeRef scope) :
	    ScopeProvider(std::move(scope)),
	    m_decl(std::move(decl)),
	    m_end(std::move(end)) {
	}

	const std::string & ForLoop::GetIdentName() const {
	    return dynamic_cast<DeclImmutIntVar &>(*m_decl).GetIdentName();
	}

	DeclImmutIntVar::DeclImmutIntVar(NodeRef ident, NodeRef value) :
	    m_ident(std::move(ident)),
	    m_value(std::move(value)) {}

	const std::string & DeclImmutIntVar::GetIdentName() const {
	    return dynamic_cast<Ident &>(*m_ident).GetName();
	}

	// CODE GENERATION

	template <typename F>
	static llvm::AllocaInst * createEntryBlockAlloca(llvm::Function * fn,
							 F && cb,
							 llvm::LLVMContext & context,
							 const std::string & varName) {
	    llvm::IRBuilder<> tempBuilder(&fn->getEntryBlock(),
					  fn->getEntryBlock().begin());
	    return tempBuilder.CreateAlloca(cb(context), 0,
					    varName.c_str());
	}

	llvm::Value * Void::CodeGen(LLVMState & state) {
	    return nullptr;
	}

	llvm::Value * Ident::CodeGen(LLVMState & state) {
	    llvm::Value * value = state.vars[m_name].value;
	    if (!value) {
		throw std::runtime_error("reference to non-existent variable " + m_name);
	    }
	    return state.builder.CreateLoad(value, m_name.c_str());
	}
	
	llvm::Value * DeclImmutIntVar::CodeGen(LLVMState & state) {
	    const auto & varName = dynamic_cast<Ident &>(*m_ident).GetName();
	    if (state.vars.find(varName) == state.vars.end()) {
		auto fn = state.builder.GetInsertBlock()->getParent();
		auto alloca = createEntryBlockAlloca(fn, llvm::Type::getInt32Ty,
						     state.context, varName);
		state.builder.CreateStore(m_value->CodeGen(state), alloca);
		state.vars[varName] = {alloca, false};
		return state.vars[varName].value;
	    } else {
		throw std::runtime_error("declaration of variable "
					 + varName + " would shadow existing variable");
	    }
	}
	
	llvm::Value * Function::CodeGen(LLVMState & state) {
	    auto funcType = llvm::FunctionType::get(state.builder.getVoidTy(), false);
	    auto funct = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
						m_name, state.modRef.get());
	    auto fnEntry = llvm::BasicBlock::Create(state.context, "entrypoint", funct);
	    auto fnExit = llvm::BasicBlock::Create(state.context, "exitpoint", funct);
	    // TODO: for functions that return values, create an alloca...
	    state.fnExitPoint = fnExit;
	    state.builder.SetInsertPoint(fnEntry);
	    this->GetScope().CodeGen(state);
	    // state.builder.CreateBr(state.fnExitPoint);
	    state.builder.SetInsertPoint(fnExit);
	    // TODO: if a function returns something, load modified alloca here...
	    state.builder.CreateRetVoid();
	    return nullptr;
	}
	
	llvm::Value * ForLoop::CodeGen(LLVMState & state) {
	    auto fn = state.builder.GetInsertBlock()->getParent();
	    m_decl->CodeGen(state);
	    auto & varName = this->GetIdentName();
	    auto alloca = state.vars[varName].value;
	    auto loopBlock = llvm::BasicBlock::Create(state.context, "loop", fn);
	    auto loopBody = llvm::BasicBlock::Create(state.context, "loopbody", fn);
	    auto afterBlock = llvm::BasicBlock::Create(state.context, "afterloop", fn);
	    state.builder.CreateBr(loopBody);
	    state.builder.SetInsertPoint(loopBody);
	    state.stack.push(loopBlock);
	    this->GetScope().CodeGen(state);
	    state.stack.pop();
	    state.builder.SetInsertPoint(loopBlock);
	    auto stepVal = llvm::ConstantInt::get(state.context, llvm::APInt(32, 1));
	    auto endCond = m_end->CodeGen(state);
	    auto currVar = state.builder.CreateLoad(alloca, varName.c_str());
	    auto nextVar = state.builder.CreateFAdd(currVar, stepVal, "nextvar");
	    state.builder.CreateStore(nextVar, alloca);
	    endCond = state.builder.CreateICmpNE(endCond, nextVar, "loopcond");
	    state.builder.CreateCondBr(endCond, loopBody, afterBlock);
	    state.builder.SetInsertPoint(afterBlock);
	    state.vars.erase(varName);
	    return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(state.context));
	}

	llvm::Value * Return::CodeGen(LLVMState & state) {
	    throw std::runtime_error("Explicit call to Return::CodeGen");
	    // This function is to be removed...
	    return nullptr;
	}
	
	llvm::Value * Scope::CodeGen(LLVMState & state) {
	    bool foundRet = false;
	    for (auto & child : m_children) {
		Return * ret = dynamic_cast<Return *>(child.get());
		if (ret) {
		    state.builder.CreateBr(state.fnExitPoint);
		    foundRet = true;
		} else {
		    child->CodeGen(state);
		}
	    }
	    if (!state.stack.empty() && !foundRet) {
		state.builder.CreateBr(state.stack.top());
	    }
	    return nullptr;
	}

	llvm::Value * Integer::CodeGen(LLVMState & state) {
	    uint64_t longVal = static_cast<uint64_t>(m_value);
	    return llvm::ConstantInt::get(state.context, llvm::APInt(32, longVal));
	}
    }
}

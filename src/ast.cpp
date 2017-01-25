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
	    ScopeProvider(std::move(scope)), m_name(name), m_returnType(returnType) {
	}
	
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

	// I found this helper function in a tutorial from the LLVM page. Creating
	// a new IRBuilder looks a little suspicious to me, but maybe it's ok. I'll
	// see about replacing this when I understand the API better.
	template <typename F>
	static llvm::AllocaInst * CreateEntryBlockAlloca(llvm::Function * fn,
							 F && cb,
							 llvm::LLVMContext & context,
							 const std::string & varName = "") {
	    llvm::IRBuilder<> tempBuilder(&fn->getEntryBlock(),
					  fn->getEntryBlock().begin());
	    return tempBuilder.CreateAlloca(cb(context), 0,
					    varName.c_str());
	}

	llvm::Value * MultOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    if (state.parentExprType == "int") {
		return state.builder.CreateMul(lhs, rhs);
	    } else {
		throw std::runtime_error("type cannot be multiplied");
	    }
	    return nullptr;
	}

	llvm::Value * DivOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    if (state.parentExprType == "int") {
		return state.builder.CreateSDiv(lhs, rhs);
	    } else {
		throw std::runtime_error("type cannot be divided");
	    }
	    return nullptr;
	}

	llvm::Value * AddOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    if (state.parentExprType == "int") {
		return state.builder.CreateAdd(lhs, rhs);
	    } else {
		throw std::runtime_error("type cannot be added");
	    }
	    return nullptr;
	}

	llvm::Value * SubOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    if (state.parentExprType == "int") {
		return state.builder.CreateSub(lhs, rhs);
	    } else {
		throw std::runtime_error("type cannot be subtracted");
	    }
	    return nullptr;
	}

	llvm::Value * Void::CodeGen(LLVMState & state) {
	    return nullptr;
	}

	llvm::Value * Ident::CodeGen(LLVMState & state) {
	    llvm::Value * value = state.vars[m_name];
	    return state.builder.CreateLoad(value, m_name.c_str());
	}

	llvm::Value * Expr::CodeGen(LLVMState & state) {
	    state.parentExprType = m_type;
	    return m_tree->CodeGen(state);
	}
	
	llvm::Value * DeclImmutIntVar::CodeGen(LLVMState & state) {
	    const auto & varName = dynamic_cast<Ident &>(*m_ident).GetName();
	    auto fn = state.builder.GetInsertBlock()->getParent();
	    auto alloca = CreateEntryBlockAlloca(fn, llvm::Type::getInt32Ty,
						 state.context, varName);
	    state.builder.CreateStore(m_value->CodeGen(state), alloca);
	    state.vars[varName] = alloca;
	    return state.vars[varName];
	}
	
	llvm::Value * Function::CodeGen(LLVMState & state) {
	    llvm::FunctionType * funcType = nullptr;
	    if (m_returnType == "void") {
		state.currentFnInfo.exitValue = nullptr;
		funcType = llvm::FunctionType::get(state.builder.getVoidTy(), false);
	    } else if (m_returnType == "int") {
		funcType = llvm::FunctionType::get(state.builder.getInt32Ty(), false);
	    } // other types...
	    auto funct = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
						m_name, state.modRef.get());
	    auto fnEntry = llvm::BasicBlock::Create(state.context, "entrypoint", funct);
	    auto fnExit = llvm::BasicBlock::Create(state.context, "exitpoint", funct);
	    state.currentFnInfo.exitPoint = fnExit;
	    state.builder.SetInsertPoint(fnEntry);
	    static const std::string exitVarName = "exitcode";
	    if (m_returnType == "int") {
		state.currentFnInfo.exitValue =
		    CreateEntryBlockAlloca(funct, llvm::Type::getInt32Ty, state.context,
					   exitVarName);
	    } // other types...
	    this->GetScope().CodeGen(state);
	    state.builder.SetInsertPoint(fnExit);
	    if (m_returnType == "void") {
		state.builder.CreateRetVoid();
	    } else if (m_returnType == "int") {
		auto exitValue = state.builder.CreateLoad(state.currentFnInfo.exitValue,
							  exitVarName);
		state.builder.CreateRet(exitValue);
	    }
	    return nullptr;
	}
	
	llvm::Value * ForLoop::CodeGen(LLVMState & state) {
	    auto fn = state.builder.GetInsertBlock()->getParent();
	    m_decl->CodeGen(state);
	    auto & varName = this->GetIdentName();
	    auto alloca = state.vars[varName];
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
	    return m_value->CodeGen(state);
	}
	
	llvm::Value * Scope::CodeGen(LLVMState & state) {
	    bool foundRet = false;
	    for (auto & child : m_children) {
		Return * ret = dynamic_cast<Return *>(child.get());
		if (ret) {
		    // The parser does not generate nodes for statements after
		    // a return, so it's safe to assume here that encountering
		    // a return means the end of a BB
		    if (state.currentFnInfo.exitValue) {
			state.builder.CreateStore(ret->CodeGen(state),
						  state.currentFnInfo.exitValue);
		    }
		    state.builder.CreateBr(state.currentFnInfo.exitPoint);
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

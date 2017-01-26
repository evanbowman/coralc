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
	    ScopeProvider(std::move(scope)), m_name(name),
	    m_returnType(returnType) {}
	
	const std::string & Ident::GetName() const {
	    return m_name;
	}

	Boolean::Boolean(const bool value) : m_value(value) {}
	
	Return::Return(NodeRef value) : m_value(std::move(value)) {}
	
	Integer::Integer(const int value) : m_value(value) {}

	Float::Float(const float value) : m_value(value) {}
	
	ForLoop::ForLoop(NodeRef decl, NodeRef end, ScopeRef scope) :
	    ScopeProvider(std::move(scope)),
	    m_decl(std::move(decl)),
	    m_end(std::move(end)) {
	}

	const std::string & ForLoop::GetIdentName() const {
	    return dynamic_cast<DeclIntVar &>(*m_decl).GetIdentName();
	}

	DeclVar::DeclVar(NodeRef ident, NodeRef value) :
	    m_ident(std::move(ident)),
	    m_value(std::move(value)) {}
	
	DeclIntVar::DeclIntVar(NodeRef ident, NodeRef value) :
	    DeclVar(std::move(ident), std::move(value)) {}

	DeclBooleanVar::DeclBooleanVar(NodeRef ident, NodeRef value) :
	    DeclVar(std::move(ident), std::move(value)) {}
	
	DeclFloatVar::DeclFloatVar(NodeRef ident, NodeRef value) :
	    DeclVar(std::move(ident), std::move(value)) {}
	
	// CODE GENERATION

	// I found this helper function in a tutorial from the LLVM site. Creating
	// a new IRBuilder looks a little suspicious to me, but I read somewhere in
	// the LLVM mailing list that doing allocations early is a good idea, so I'll
	// keep using it.
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
	    if (m_resultType == "int") {
		return state.builder.CreateMul(lhs, rhs);
	    } else if (m_resultType == "float") {
		return state.builder.CreateFMul(lhs, rhs);
	    } else {
		throw std::runtime_error("type cannot be multiplied");
	    }
	    return nullptr;
	}

	llvm::Value * DivOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    if (m_resultType == "int") {
		return state.builder.CreateSDiv(lhs, rhs);
	    } else if (m_resultType == "float") {
		return state.builder.CreateFDiv(lhs, rhs);
	    } else {
		throw std::runtime_error("type cannot be divided");
	    }
	    return nullptr;
	}

	static const std::string equalityTag = "equality test";
	
	llvm::Value * InequalityOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    llvm::Value * ret = nullptr;
	    if (m_resultType == "int") {
		ret = state.builder.CreateICmpNE(lhs, rhs, equalityTag);
	    } else if (m_resultType == "float") {
		ret = state.builder.CreateFCmpONE(lhs, rhs, equalityTag);
	    } else if (m_resultType == "bool") {
		ret = state.builder.CreateICmpNE(lhs, rhs, equalityTag);
	    } else {
		throw std::runtime_error("type cannot be compared");
	    }
	    return state.builder.CreateIntCast(ret, llvm::Type::getInt8Ty(state.context), true);
	}

	llvm::Value * EqualityOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    llvm::Value * ret = nullptr;
	    if (m_resultType == "int") {
		ret = state.builder.CreateICmpEQ(lhs, rhs, equalityTag);
	    } else if (m_resultType == "float") {
	        ret = state.builder.CreateFCmpOEQ(lhs, rhs, equalityTag);
	    } else if (m_resultType == "bool") {
	        ret = state.builder.CreateICmpEQ(lhs, rhs, equalityTag);
	    } else {
		throw std::runtime_error("type cannot be compared");
	    }
	    // I've had trouble with the llvm assembler and single bit bools,
	    // so I've been casting them to 8 bit integers.
	    return state.builder.CreateIntCast(ret, llvm::Type::getInt8Ty(state.context), true);
	}

	llvm::Value * LogicalAndOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    // Note: I use 255 and not 1 as boolean true. This is because I cast comparison
	    // results to eight bit signed, see above comment.
	    auto boolTrue = llvm::ConstantInt::get(state.context, llvm::APInt(8, 255));
	    auto isLhsTrue =
		state.builder.CreateICmpEQ(lhs, boolTrue);
	    auto isRhsTrue =
		state.builder.CreateICmpEQ(rhs, boolTrue);
	    auto bothTrue = state.builder.CreateAnd(isLhsTrue, isRhsTrue);
	    return state.builder.CreateIntCast(bothTrue, llvm::Type::getInt8Ty(state.context), true);
	}

	llvm::Value * LogicalOrOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    auto boolTrue = llvm::ConstantInt::get(state.context, llvm::APInt(8, 255));
	    auto isLhsTrue =
		state.builder.CreateICmpEQ(lhs, boolTrue);
	    auto isRhsTrue =
		state.builder.CreateICmpEQ(rhs, boolTrue);
	    auto eitherTrue = state.builder.CreateOr(isLhsTrue, isRhsTrue);
	    return state.builder.CreateIntCast(eitherTrue, llvm::Type::getInt8Ty(state.context), true);
	}

	llvm::Value * AddOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    if (m_resultType == "int") {
		return state.builder.CreateAdd(lhs, rhs);
	    } else if (m_resultType == "float") {
		return state.builder.CreateFAdd(lhs, rhs);
	    } else {
		throw std::runtime_error("type cannot be added");
	    }
	    return nullptr;
	}

	llvm::Value * SubOp::CodeGen(LLVMState & state) {
	    auto lhs = m_lhs->CodeGen(state);
	    auto rhs = m_rhs->CodeGen(state);
	    if (m_resultType == "int") {
		return state.builder.CreateSub(lhs, rhs);
	    } else if (m_resultType == "float") {
		return state.builder.CreateFSub(lhs, rhs);
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
	    return m_exprSubTree->CodeGen(state);
	}
	
	llvm::Value * DeclIntVar::CodeGen(LLVMState & state) {
	    const auto & varName = dynamic_cast<Ident &>(*m_ident).GetName();
	    auto fn = state.builder.GetInsertBlock()->getParent();
	    auto alloca = CreateEntryBlockAlloca(fn, llvm::Type::getInt32Ty,
						 state.context, varName);
	    state.builder.CreateStore(m_value->CodeGen(state), alloca);
	    state.vars[varName] = alloca;
	    return state.vars[varName];
	}

	llvm::Value * DeclFloatVar::CodeGen(LLVMState & state) {
	    const auto & varName = dynamic_cast<Ident &>(*m_ident).GetName();
	    auto fn = state.builder.GetInsertBlock()->getParent();
	    auto alloca = CreateEntryBlockAlloca(fn, llvm::Type::getFloatTy,
						 state.context, varName);
	    state.builder.CreateStore(m_value->CodeGen(state), alloca);
	    state.vars[varName] = alloca;
	    return state.vars[varName];
	}

	llvm::Value * DeclBooleanVar::CodeGen(LLVMState & state) {
	    const auto & varName = dynamic_cast<Ident &>(*m_ident).GetName();
	    auto fn = state.builder.GetInsertBlock()->getParent();
	    auto alloca = CreateEntryBlockAlloca(fn, llvm::Type::getInt8Ty,
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
	    } else if (m_returnType == "float") {
		funcType = llvm::FunctionType::get(state.builder.getFloatTy(), false);
	    } else if (m_returnType == "bool") {
		funcType = llvm::FunctionType::get(state.builder.getInt8Ty(), false);
	    } else {
		throw std::runtime_error("functions of " + m_returnType + "are not supported");
	    }
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
	    } else if (m_returnType == "float") {
		state.currentFnInfo.exitValue =
		    CreateEntryBlockAlloca(funct, llvm::Type::getFloatTy, state.context,
					   exitVarName);
	    } else if (m_returnType == "bool") {
		state.currentFnInfo.exitValue =
		    CreateEntryBlockAlloca(funct, llvm::Type::getInt8Ty, state.context,
					   exitVarName);
	    }
	    this->GetScope().CodeGen(state);
	    state.builder.SetInsertPoint(fnExit);
	    if (m_returnType == "void") {
		state.builder.CreateRetVoid();
	    } else if (m_returnType == "int" || m_returnType == "float" || m_returnType == "bool") {
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

	llvm::Value * Boolean::CodeGen(LLVMState & state) {
	    return llvm::ConstantInt::get(state.context, llvm::APInt(8, m_value));
	}

	llvm::Value * Float::CodeGen(LLVMState & state) {
	    return llvm::ConstantFP::get(state.context, llvm::APFloat(m_value));
	}
    }
}

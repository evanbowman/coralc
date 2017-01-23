#include "ast.hpp"

namespace coralc {
    namespace ast {
	void Block::AddChild(NodeRef child) {
	    m_children.push_back(std::move(child));
	}

	Ident::Ident(const std::string & name) : m_name(name) {}

	const std::string & Ident::GetName() const {
	    return m_name;
	}
	
	Integer::Integer(const int value) : m_value(value) {}

	ForLoop::ForLoop(NodeRef decl, NodeRef end, NodeRef block) :
	    m_decl(std::move(decl)),
	    m_end(std::move(end)),
	    m_block(std::move(block)) {
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
					 + varName + " would shadow existing varaible");
	    }
	}
	
	llvm::Value * ForLoop::CodeGen(LLVMState & state) {
	    auto fn = state.builder.GetInsertBlock()->getParent();
	    m_decl->CodeGen(state);
	    auto & varName = this->GetIdentName();
	    auto alloca = state.vars[varName].value;
	    auto loopBlock = llvm::BasicBlock::Create(state.context, "forloop", fn);
	    state.builder.CreateBr(loopBlock);
	    state.builder.SetInsertPoint(loopBlock);
	    m_block->CodeGen(state);
	    auto stepVal = llvm::ConstantInt::get(state.context, llvm::APInt(32, 1));
	    auto endCond = m_end->CodeGen(state);
	    auto currVar = state.builder.CreateLoad(alloca, varName.c_str());
	    auto nextVar = state.builder.CreateFAdd(currVar, stepVal, "nextvar");
	    state.builder.CreateStore(nextVar, alloca);
	    endCond = state.builder.CreateICmpNE(endCond, nextVar, "loopcond");
	    auto afterBlock =
		llvm::BasicBlock::Create(state.context, "afterloop", fn);
	    state.builder.CreateCondBr(endCond, loopBlock, afterBlock);
	    state.builder.SetInsertPoint(afterBlock);
	    state.vars.erase(varName);
	    return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(state.context));
	}
	
	llvm::Value * Block::CodeGen(LLVMState & state) {
	    for (auto & child : m_children) {
		child->CodeGen(state);
	    }
	    return nullptr;
	}

	llvm::Value * Integer::CodeGen(LLVMState & state) {
	    uint64_t longVal = static_cast<uint64_t>(m_value);
	    return llvm::ConstantInt::get(state.context, llvm::APInt(32, longVal));
	}
    }
}

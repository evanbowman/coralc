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
	    return dynamic_cast<DeclImmutVar &>(*m_decl).GetIdentName();
	}

	DeclImmutVar::DeclImmutVar(NodeRef ident, NodeRef value) :
	    m_ident(std::move(ident)),
	    m_value(std::move(value)) {}

	const std::string & DeclImmutVar::GetIdentName() const {
	    return dynamic_cast<Ident &>(*m_ident).GetName();
	}

	// CODE GENERATION

	llvm::Value * Ident::CodeGen(LLVMState & state) {
	    llvm::Value * value = state.vars[m_name].value;
	    if (!value) {
		throw std::runtime_error("reference to non-existent variable " + m_name);
	    }
	    return value;
	}
	
	llvm::Value * DeclImmutVar::CodeGen(LLVMState & state) {
	    const auto & varName = dynamic_cast<Ident &>(*m_ident).GetName();
	    if (state.vars.find(varName) == state.vars.end()) {
		state.vars[varName].value = m_value->CodeGen(state);
		return state.vars[varName].value;
	    } else {
		throw std::runtime_error("declaration of variable " + varName + " would shadow existing varaible");
	    }
	}
	
	llvm::Value * ForLoop::CodeGen(LLVMState & state) {
	    auto start = m_decl->CodeGen(state);
	    auto fn = state.builder.GetInsertBlock()->getParent();
	    auto header = state.builder.GetInsertBlock();
	    auto loopBlock = llvm::BasicBlock::Create(state.context, "forloop", fn);
	    state.builder.CreateBr(loopBlock);
	    state.builder.SetInsertPoint(loopBlock);
	    auto var =
		state.builder.CreatePHI(llvm::Type::getInt32Ty(state.context), 2,
					this->GetIdentName());
	    var->addIncoming(start, header);
	    m_block->CodeGen(state);
	    auto stepVal = llvm::ConstantInt::get(state.context, llvm::APInt(32, 1));
	    auto nextVar = state.builder.CreateFAdd(var, stepVal, "nextvar");
	    auto end = m_end->CodeGen(state);
	    end = state.builder.CreateICmpNE(end, nextVar, "loopcond");
	    auto loopEndBlock = state.builder.GetInsertBlock();
	    auto afterBlock =
		llvm::BasicBlock::Create(state.context, "afterloop", fn);
	    state.builder.CreateCondBr(end, loopBlock, afterBlock);
	    state.builder.SetInsertPoint(afterBlock);
	    var->addIncoming(nextVar, loopEndBlock);
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

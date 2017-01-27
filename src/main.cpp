#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "Parser.hpp"
#include <sstream>
#include <fstream>

namespace coralc {
    void GenerateCode(ast::NodeRef & root, const std::string & fname) {
        LLVMState state;
	try {
	    root->CodeGen(state);
	} catch (const std::exception & ex) {
	    std::cerr << ex.what() << std::endl;
	    exit(EXIT_FAILURE);
	}
	state.modRef->dump();
	auto targetTriple = llvm::sys::getDefaultTargetTriple();
	state.modRef->setTargetTriple(targetTriple);
	std::string error;
	auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
	if (!target) {
	    std::cerr << error;
	    return;
	}
	auto CPU = "generic";
	auto features = "";
	llvm::TargetOptions opt;
	auto RM = llvm::Optional<llvm::Reloc::Model>();
	auto targetMachine =
	    target->createTargetMachine(targetTriple, CPU, features, opt, RM);
	state.modRef->setDataLayout(targetMachine->createDataLayout());
	auto outputName = fname + ".o";
	std::error_code EC;
	llvm::raw_fd_ostream dest(outputName, EC, llvm::sys::fs::F_None);
	if (EC) {
	    std::cerr << "Could not open file: " << EC.message();
	    return ;
	}
	llvm::legacy::PassManager pass;
	auto FileType = llvm::TargetMachine::CGFT_ObjectFile;
	if (targetMachine->addPassesToEmitFile(pass, dest, FileType)) {
	    std::cerr << "TheTargetMachine can't emit a file of this type";
	    return;
	}
	pass.run(*state.modRef);
	dest.flush();
    }
}

int main(int argc, char ** argv) {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
    if (argc > 1) {
	std::ifstream t(argv[1]);
	std::stringstream buffer;
	buffer << t.rdbuf();
	coralc::Parser parser;
	try {
	    coralc::ast::NodeRef root = parser.Parse(buffer.str());
	    coralc::GenerateCode(root, argv[1]);
	} catch (const std::exception & ex) {
	    std::cerr << ex.what() << " for file " << argv[1] << std::endl;
	    return EXIT_FAILURE;
	}
    } else {
	// Print usage. Transition to getopt...
    }
}

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
	//state.modRef->dump();
	std::string err;
	auto output = fname + ".bc";
	std::error_code ec;
	llvm::raw_fd_ostream dest(output, ec, llvm::sys::fs::F_None);
	if (ec) {
	    throw std::runtime_error(ec.message());
	}
	llvm::WriteBitcodeToFile(state.modRef.get(), dest);
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

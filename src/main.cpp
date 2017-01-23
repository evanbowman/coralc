#include "Parser.hpp"
#include <sstream>
#include <fstream>

namespace coralc {
    void GenerateCode(ast::NodeRef & root) {
        LLVMState state;	 
	try {
	    root->CodeGen(state);
	} catch (const std::exception & ex) {
	    std::cout << ex.what() << std::endl;
	    exit(EXIT_FAILURE);
	}
    }
}

int main(int argc, char ** argv) {
    if (argc > 1) {
	std::ifstream t(argv[1]);
	std::stringstream buffer;
	buffer << t.rdbuf();
	coralc::Parser parser;
	try {
	    coralc::ast::NodeRef root = parser.Parse(buffer.str());
	    coralc::GenerateCode(root);
	} catch (const std::exception & ex) {
	    std::cerr << ex.what() << " for file" << argv[1] << std::endl;
	    return EXIT_FAILURE;
	}
    } else {
	// Print usage. Transition to getopt...
    }
}

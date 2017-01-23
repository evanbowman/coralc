CC = clang++
CXXFLAGS = $(shell llvm-config --cxxflags) -O2 -std=c++14
LDFLAGS = $(shell llvm-config --ldflags --libs) -lz -lcurses -lm -lcoral-lexer

EXEC = coralc
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) $(LDFLAGS)

%.o: %.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f $(EXEC) $(OBJECTS)

test:
	./coralc ~/Desktop/test.crl > test.ll
	llc test.ll
	clang -c test.s -o test.o
	clang test.o -o test

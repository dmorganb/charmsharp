CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude

LIB_SOURCES = src/lexer.cpp src/translator.cpp
HEADERS = src/lexer.hpp include/charmsharp/translator.hpp

.PHONY: all test charm-check clean
all: build/charmsharp

build/charmsharp: src/main.cpp $(LIB_SOURCES) $(HEADERS)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp $(LIB_SOURCES) -o $@

build/tests: tests/tests.cpp $(LIB_SOURCES) $(HEADERS)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/tests.cpp $(LIB_SOURCES) -o $@

test: build/tests build/charmsharp
	./build/tests
	sh tests/cli_smoke.sh ./build/charmsharp examples/Counter.csh

charm-check: build/charmsharp
	sh tests/charm_smoke.sh ./build/charmsharp examples/Counter.csh

clean:
	rm -f build/charmsharp build/tests

CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude

LIB_SOURCES = src/lexer.cpp src/translator.cpp
HEADERS = src/lexer.hpp include/charmsharp/translator.hpp

.PHONY: all test charm-check charm-run charm-objects charm-threaded charm-message clean
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

charm-run: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Hello.csh

charm-objects: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Objects.csh Objects 'Hello from array Cell'

charm-threaded: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Threaded.csh Threaded 'Hello from threaded entry'

charm-message: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Message.csh Message 'Hello from message: 42'

clean:
	rm -f build/charmsharp build/tests

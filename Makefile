CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude

LIB_SOURCES = src/lexer.cpp src/translator.cpp
HEADERS = src/lexer.hpp include/charmsharp/translator.hpp

.PHONY: all test charm-check charm-run charm-objects charm-threaded charm-message charm-readonly charm-inheritance charm-message-inheritance charm-parallel-inheritance charm-benchmark-primes charm-benchmark-reduction charm-ctor-initializers clean
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

charm-readonly: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Readonly.csh Readonly 'Hello from readonly: 42'

charm-inheritance: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Inheritance.csh Inheritance 'Hello from inherited base: 42'

charm-message-inheritance: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/MessageInheritance.csh MessageInheritance 'Hello from inherited message: 42'

charm-parallel-inheritance: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/ParallelInheritance.csh ParallelInheritance 'Hello from inherited proxy: 42'

charm-benchmark-primes: build/charmsharp
	sh tests/benchmark_primes.sh ./build/charmsharp

charm-benchmark-reduction: build/charmsharp
	sh tests/benchmark_reduction.sh ./build/charmsharp

charm-ctor-initializers: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/ConstructorInitializers.csh ConstructorInitializers 'Hello from constructor initializers: 43'

clean:
	rm -f build/charmsharp build/tests

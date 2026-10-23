CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic

.PHONY: all test benchmark clean
all: charm-metrics

charm-metrics: main.cpp lexer.hpp metrics.hpp
	$(CXX) $(CXXFLAGS) main.cpp -o $@

lexer-tests: tests/lexer_tests.cpp lexer.hpp
	$(CXX) $(CXXFLAGS) tests/lexer_tests.cpp -o $@

metrics-tests: tests/metrics_tests.cpp metrics.hpp lexer.hpp
	$(CXX) $(CXXFLAGS) tests/metrics_tests.cpp -o $@

test: lexer-tests metrics-tests
	./lexer-tests
	./metrics-tests

benchmark: charm-metrics
	./charm-metrics benchmark --root ../../benchmarks --format text

clean:
	rm -f charm-metrics lexer-tests metrics-tests

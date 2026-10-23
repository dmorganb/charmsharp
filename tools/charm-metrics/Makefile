CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic

.PHONY: test
test:
	$(CXX) $(CXXFLAGS) tests/lexer_tests.cpp -o /tmp/charm-metrics-lexer-tests
	/tmp/charm-metrics-lexer-tests

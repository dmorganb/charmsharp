#include "charmsharp/translator.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

int checks = 0;

void equal(std::string_view actual, std::string_view expected) {
    ++checks;
    if (actual != expected) {
        std::cerr << "EXPECTED:\n" << expected << "\nACTUAL:\n" << actual << '\n';
        std::exit(1);
    }
}

void contains(std::string_view actual, std::string_view substring) {
    ++checks;
    if (actual.find(substring) == std::string_view::npos) {
        std::cerr << "missing substring: " << substring << "\nIN:\n" << actual << '\n';
        std::exit(1);
    }
}

void rejects(std::string_view input, std::string_view message) {
    ++checks;
    try {
        (void)charmsharp::translate(input, "TestModule");
    } catch (const charmsharp::Diagnostic& error) {
        contains(error.what(), message);
        return;
    }
    std::cerr << "Expected diagnostic containing: " << message << '\n';
    std::exit(1);
}

void golden_counter() {
    constexpr std::string_view input = R"SRC(#include <cstdio>
// chare Fake { entry: void broken(); }
chare Counter {
private:
    int value = 0;
public:
    int localValue() const { return value; }
entry:
    Counter();
    void increment(int amount) { value += amount; }
};
)SRC";

    const auto generated = charmsharp::translate(input, "CounterModule");
    equal(generated.interface_source, R"CI(module CounterModule {
    chare Counter {
        entry Counter();
        entry void increment(int amount);
    };
};
)CI");
    equal(generated.cpp_source, R"CPP(#include "CounterModule.decl.h"
#include <cstdio>
// chare Fake { entry: void broken(); }
class Counter : public CBase_Counter {
private:
    int value = 0;
public:
    int localValue() const { return value; }
public:
    Counter();
    void increment(int amount) { value += amount; }
};
#include "CounterModule.def.h"
)CPP");
}

void preserves_ordinary_cpp_and_literal_braces() {
    const auto output = charmsharp::translate(R"SRC(
constexpr auto punctuation = R"raw({}; entry: chare when "})raw";
int freeFunction() { return 42; }
chare A {
public:
    const char* local() { return "{ // not a comment }"; }
entry:
    void f() { /* } */ auto s = R"x(})x"; (void)s; }
private:
    int x{3};
};
)SRC", "M");
    contains(output.cpp_source, "R\"raw({}; entry: chare when \"})raw\"");
    contains(output.cpp_source, "int freeFunction() { return 42; }");
    contains(output.cpp_source, "private:\n    int x{3};");
    contains(output.interface_source, "entry void f();");
    ++checks;
    if (output.interface_source.find("local") != std::string::npos)
        throw std::runtime_error("non-entry method leaked into interface");
}

void multiple_sections_and_definitions() {
    const auto output = charmsharp::translate(R"SRC(chare Worker {
entry:
  void first();
private:
  int x = 1;
entry:
  void second(int y) { x = y; }
public:
  int local() const { return x; }
};)SRC", "Workers");
    contains(output.interface_source, "entry void first();");
    contains(output.interface_source, "entry void second(int y);");
    ++checks;
    if (output.interface_source.find("local") != std::string::npos)
        throw std::runtime_error("public should not register entry methods");
}

void multiple_chares_and_mainchare() {
    const auto output = charmsharp::translate(R"SRC(mainchare Main {
entry:
  Main(CkArgMsg* args) { delete args; }
};
chare Worker {
entry:
  Worker() { CkExit(); }
  void ping(int value) { (void)value; }
};
)SRC", "Hello");
    contains(output.interface_source, "mainmodule Hello {");
    contains(output.interface_source, "mainchare Main {\n        entry Main(CkArgMsg* args);");
    contains(output.interface_source, "chare Worker {\n        entry Worker();\n        entry void ping(int value);");
    contains(output.cpp_source, "class Main : public CBase_Main");
    contains(output.cpp_source, "class Worker : public CBase_Worker");
}

void groups_and_arrays() {
    const auto output = charmsharp::translate(R"SRC(mainchare Main {
entry:
    Main(CkArgMsg* args) { delete args; }
};
group Service {
entry:
    Service() {}
    void log(int value) { (void)value; }
};
nodegroup NodeService {
entry:
    NodeService() {}
};
array [1D] chare Cell {
entry:
    Cell() {}
    void update() {}
};
array [2 D] chare Grid {
entry:
    Grid() {}
};
)SRC", "Objects");
    contains(output.interface_source, "group Service {\n        entry Service();\n        entry void log(int value);");
    contains(output.interface_source, "nodegroup NodeService {\n        entry NodeService();");
    contains(output.interface_source, "array [1D] Cell {\n        entry Cell();\n        entry void update();");
    contains(output.interface_source, "array [2D] Grid {\n        entry Grid();");
    contains(output.cpp_source, "class Service : public CBase_Service");
    contains(output.cpp_source, "class NodeService : public CBase_NodeService");
    contains(output.cpp_source, "class Cell : public CBase_Cell");
    contains(output.cpp_source, "class Grid : public CBase_Grid");
}

void diagnostics() {
    rejects("chare A { entry: int x; };", "only simple method");
    rejects("chare A { entry: int value(); };", "only 'void name");
    rejects("chare A { entry: void f() const; };", "qualifiers");
    rejects("chare A { entry: [[charm::threaded]] void f(); };", "attributes");
    rejects("chare A { entry: void f() { when ping() { } } };", "SDAG");
    rejects("chare A : public Base { entry: void f(); };", "inheritance");
    rejects("chare A {}; chare A {};", "duplicate chare name");
    rejects("mainchare Main {}; mainchare Other {};", "only one mainchare");
    rejects("namespace N { chare A { entry: void f(); }; }", "namespace");
    rejects("chare A { entry: void f(int x = 3); };", "default entry arguments");
    rejects("chare A { entry: void f(); void f(); };", "duplicate");
    rejects("chare A { entry: void f() { }", "unbalanced");
    rejects("#if 0\nchare A {};\n#endif\n", "preprocessor conditional");
    rejects("chare A {\n#if FOO\nentry: void f();\n#endif\n};",
            "preprocessor directives between chare members");
    rejects("array [0D] chare A {};", "array dimension");
    rejects("array [2Q] chare A {};", "array dimension");
    rejects("array [2D] A {};", "expected 'chare'");
    rejects("group A {}; nodegroup A {};", "duplicate chare name");
    rejects("chare A { entry: void f() { auto p = R\"(unterminated); } };",
            "unterminated raw string");

    const std::string source = "\n\nchare A {\nentry:\n  int value;\n};";
    try {
        (void)charmsharp::translate(source, "M");
        throw std::runtime_error("expected a location diagnostic");
    } catch (const charmsharp::Diagnostic& error) {
        contains(charmsharp::format_diagnostic(source, error), "5:3:");
    }
}

} // namespace

int main() {
    golden_counter();
    preserves_ordinary_cpp_and_literal_braces();
    multiple_sections_and_definitions();
    multiple_chares_and_mainchare();
    groups_and_arrays();
    diagnostics();
    std::cout << "PASS " << checks << " checks\n";
}

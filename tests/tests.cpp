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

void attributed_entries() {
    const auto output = charmsharp::translate(R"SRC(group Worker {
entry:
    [[charm::threaded]] void compute(const int* values, int count) {
        (void)values;
        (void)count;
    }
    [[charm::sync]] Reply* fetch();
    void notify(CkReductionMsg* msg);
};)SRC", "Attributes");
    contains(output.interface_source, "entry [threaded] void compute(const int* values, int count);");
    contains(output.interface_source, "entry [sync] Reply* fetch();");
    contains(output.interface_source, "entry void notify(CkReductionMsg* msg);");
    contains(output.cpp_source, "void compute(const int* values, int count)");
    contains(output.cpp_source, "Reply* fetch();");
    ++checks;
    if (output.cpp_source.find("[[charm::") != std::string::npos)
        throw std::runtime_error("Charm# entry attributes leaked into C++ output");
}

void fixed_size_messages() {
    const auto output = charmsharp::translate(R"SRC(message Payload {
public:
    int count = 0;
    double weights[3]{};
    int total() const { return count; }
};
mainchare Main {
entry:
    Main(CkArgMsg* args) { delete args; }
    void accept(Payload* msg) { delete msg; }
};
)SRC", "Messages");
    contains(output.interface_source, "mainmodule Messages {\n    message Payload;\n");
    contains(output.interface_source, "entry void accept(Payload* msg);");
    contains(output.cpp_source, "class Payload : public CMessage_Payload {");
    contains(output.cpp_source, "double weights[3]{};");
    contains(output.cpp_source, "int total() const { return count; }");

    const auto message_only = charmsharp::translate("message M { int x; };", "Only");
    contains(message_only.interface_source, "module Only {\n    message M;");
    contains(message_only.cpp_source, "class M : public CMessage_M");

    // Emission order must satisfy entry method references even if the source
    // places the message declaration after its use.
    const auto reordered = charmsharp::translate(
        "chare A { entry: void receive(M* msg); }; message M { int x; };", "Order");
    contains(reordered.interface_source, "module Order {\n    message M;\n    chare A");
}

void diagnostics() {
    rejects("chare A { entry: int x; };", "only simple method");
    rejects("chare A { entry: int value(); };", "must return void");
    rejects("chare A { entry: void f() const; };", "qualifiers");
    rejects("chare A { entry: [[charm::unknown]] void f(); };", "unsupported Charm++ entry attribute");
    rejects("chare A { entry: [[charm::threaded]] A(); };", "entry constructors cannot");
    rejects("chare A { entry: [[charm::sync]] void f(); };", "sync entries must return");
    rejects("chare A { entry: [[charm::threaded]] [[charm::sync]] void f(); };", "cannot be combined");
    rejects("chare A { entry: [[charm::threaded]] [[charm::threaded]] void f(); };", "duplicate Charm++");
    rejects("chare A { entry: [[other::threaded]] void f(); };", "expected [[charm::");
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
    rejects("message { int value; };", "message type name");
    rejects("message M;", "expected '{'");
    rejects("message M { int value; }", "expected ';'");
    rejects("message M : Base {};", "message inheritance");
    rejects("message M { int values[]; };", "variable-size message arrays");
    rejects("message M { entry: void foo(); };", "entry: is only valid");
    rejects("message M {}; message M {};", "duplicate");
    rejects("message M {}; chare M {};", "duplicate");
    rejects("namespace N { message M {}; }", "namespace messages");
    rejects("#ifdef X\nmessage M {};\n#endif", "preprocessor conditional");

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
    attributed_entries();
    fixed_size_messages();
    diagnostics();
    std::cout << "PASS " << checks << " checks\n";
}

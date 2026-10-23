#include "charmsharp/translator.hpp"
#include "../src/sdag.hpp"

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


void reduction_target_entries() {
    const auto output = charmsharp::translate(R"SRC(mainchare Main {
entry:
    Main(CkArgMsg* args) { delete args; }
    [[charm::reductiontarget]] void printResult(int result) {
        (void)result;
        CkExit();
    }
};)SRC", "Reduction");
    contains(output.interface_source, "entry [reductiontarget] void printResult(int result);");
    contains(output.cpp_source, "void printResult(int result)");
    ++checks;
    if (output.cpp_source.find("[[charm::reductiontarget]]") != std::string::npos)
        throw std::runtime_error("reductiontarget leaked into generated C++");

    rejects("chare A { entry: [[charm::reductiontarget]] A(); };",
            "entry constructors cannot");
    rejects("chare A { entry: [[charm::reductiontarget]] [[charm::reductiontarget]] void f(); };",
            "duplicate Charm++");
    rejects("chare A { entry: [[charm::sync]] [[charm::reductiontarget]] void f(); };",
            "reductiontarget cannot");
    rejects("chare A { entry: [[charm::threaded]] [[charm::reductiontarget]] void f(); };",
            "reductiontarget cannot");
}


void entry_constructor_initializers() {
    const auto generated = charmsharp::translate(R"SRC(struct Base {
    explicit Base(int x) : value(x) {}
    int value;
};
chare Worker : public Base {
    int count;
    int values[2];
entry:
    Worker(int n) : Base(n), count{n + 1}, values{1, 2} {
        count += values[0];
    }
    void ping() {}
};)SRC", "Ctor");
    contains(generated.interface_source, "entry Worker(int n);");
    contains(generated.interface_source, "entry void ping();");
    contains(generated.cpp_source,
             "Worker(int n) : Base(n), count{n + 1}, values{1, 2} {");
    ++checks;
    if (generated.interface_source.find("count{") != std::string::npos)
        throw std::runtime_error("constructor initializer leaked into .ci");

    const auto nested = charmsharp::translate(R"SRC(mainchare Main {
    int value;
entry:
    Main(CkArgMsg* m) : value((1 + (2 * 3))) { delete m; CkExit(); }
};)SRC", "Nested");
    contains(nested.interface_source, "entry Main(CkArgMsg* m);");
    contains(nested.cpp_source, "value((1 + (2 * 3)))");

    rejects("chare A { entry: void f() : x(1) {}; };",
            "initializer lists are only valid for entry constructors");
    rejects("chare A { entry: A() : x; };",
            "expected '(' or '{'");
    rejects("chare A { entry: A() : x(1); };",
            "expected constructor body");
    rejects("chare A { entry: A() : x(1), {}; };",
            "expected initializer member");
    rejects("chare A { entry: A() : x(1), y(2); };",
            "expected constructor body");
}


void marshalled_entry_arrays() {
    const auto output = charmsharp::translate(R"SRC(mainchare Main {
entry:
    Main(CkArgMsg* msg) { delete msg; }
};
chare Worker {
entry:
    void receive(int n, float data[n]) {
        float value = data[0];
        (void)value;
    }
    void fixed(double data[3]) { (void)data[0]; }
    void pointer(int n, float* data) { (void)n; (void)data; }
};)SRC", "Arrays");
    contains(output.interface_source, "entry void receive(int n, float data[n]);");
    contains(output.interface_source, "entry void fixed(double data[3]);");
    contains(output.interface_source, "entry void pointer(int n, float* data);");
    contains(output.cpp_source, "void receive(int n, float *data) {");
    contains(output.cpp_source, "void fixed(double *data) {");
    contains(output.cpp_source, "void pointer(int n, float* data) {");
    contains(output.cpp_source, "float value = data[0];");

    const auto declared = charmsharp::translate(R"SRC(chare Worker {
entry:
    void receive(int n, float data[n]);
};
void Worker::receive(int n, float data[n]) {
    (void)data[0];
})SRC", "OutOfLine");
    contains(declared.interface_source, "entry void receive(int n, float data[n]);");
    contains(declared.cpp_source, "void receive(int n, float *data);");
    contains(declared.cpp_source, "void Worker::receive(int n, float *data) {");

    rejects("chare A { entry: void send(int n, float data[]); };",
            "unsupported marshalled array");
    rejects("chare A { entry: void send(int n, float data[n + 1]); };",
            "unsupported marshalled array");
    rejects("chare A { entry: void send(float data[n], int n); };",
            "earlier integer parameter");
    rejects("chare A { entry: void send(float data[0]); };",
            "literal bound must be positive");
    rejects("chare A { entry: void send(float data[-1]); };",
            "unsupported marshalled array");
    rejects("chare A { entry: void send(int n, float data[n][2]); };",
            "unsupported marshalled array");
    rejects("chare A { entry: void send(int n, float* data[n]); };",
            "unsupported marshalled array");
    rejects("chare A { entry: void send(int n, Widget data[n]); };",
            "unsupported marshalled array element type");
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

void readonly_globals() {
    const auto output = charmsharp::translate(R"SRC(readonly int threshold;
readonly double weights[3];
mainchare Main {
entry:
    Main(CkArgMsg* args) { delete args; threshold = 42; }
};
)SRC", "ReadOnly");
    contains(output.interface_source, "readonly int threshold;");
    contains(output.interface_source, "readonly double weights[3];");
    contains(output.cpp_source, "int threshold;");
    contains(output.cpp_source, "double weights[3];");
    ++checks;
    if (output.cpp_source.find("readonly") != std::string::npos)
        throw std::runtime_error("readonly keyword leaked into C++ output");
    const auto alone = charmsharp::translate("readonly int size;", "Globals");
    contains(alone.interface_source, "module Globals {\n    readonly int size;");
}

void ordinary_inheritance() {
    const auto output = charmsharp::translate(R"SRC(class Logging {
public:
    void log() {}
};
struct Metrics { int calls = 0; };
chare Worker : public Logging, private Metrics {
entry:
    Worker() { log(); }
    void work() { log(); }
};
group Service : protected Logging {
entry:
    Service() { log(); }
};
)SRC", "Inheritance");
    contains(output.cpp_source,
             "class Worker : public CBase_Worker, public Logging, private Metrics {");
    contains(output.cpp_source, "class Service : public CBase_Service, protected Logging {");
    contains(output.interface_source, "chare Worker {");
    contains(output.interface_source, "group Service {");
    ++checks;
    if (output.interface_source.find("Logging") != std::string::npos)
        throw std::runtime_error("ordinary C++ base leaked into .ci");

    const auto default_access = charmsharp::translate(
        "struct Domain {}; chare A : Domain { entry: A() {} };", "Access");
    contains(default_access.cpp_source, "class A : public CBase_A, Domain {");
}

void message_inheritance() {
    const auto output = charmsharp::translate(R"SRC(struct Metadata {
    int origin = 40;
};
class Flags { protected: int adjustment = 2; };
message Payload : public Metadata, protected Flags {
public:
    int result() const { return origin + adjustment; }
};
mainchare Main { entry: Main(CkArgMsg* args) { delete args; } };
)SRC", "MessageBases");
    contains(output.interface_source, "message Payload;");
    contains(output.cpp_source,
             "class Payload : public CMessage_Payload, public Metadata, protected Flags {");
    contains(output.cpp_source, "int result() const { return origin + adjustment; }");
    ++checks;
    if (output.interface_source.find("Metadata") != std::string::npos)
        throw std::runtime_error("ordinary message base leaked into .ci");
}

void parallel_inheritance() {
    const auto output = charmsharp::translate(R"SRC(class Audit { public: int code() const { return 42; } };
chare Base {
entry:
    Base() {}
    void ping() {}
};
chare Derived : public Base, public Audit {
entry:
    Derived() {}
    void pong() { (void)code(); }
};
)SRC", "InheritedProxy");
    contains(output.interface_source, "chare Derived : Base {");
    contains(output.interface_source, "entry void pong();");
    contains(output.cpp_source,
             "class Derived : public CBase_Derived, public Audit {");
    ++checks;
    if (output.cpp_source.find("CBase_Derived, public Base") != std::string::npos)
        throw std::runtime_error("parallel base was duplicated in C++");

    const auto reversed = charmsharp::translate(R"SRC(struct App {};
chare Parent { entry: Parent() {} };
chare Child : public App, public Parent { entry: Child() {} };
)SRC", "Mixed");
    contains(reversed.interface_source, "chare Child : Parent {");
    contains(reversed.cpp_source, "class Child : public CBase_Child, public App {");
}


void sdag_structure() {
    using charmsharp::detail::SdagKind;
    constexpr std::string_view source = R"SRC({
        for (dist = 1; dist < numElements; dist <<= 1) {
            if (thisIndex + dist < numElements) serial {
                prefixArray[thisIndex + dist].passValue(dist, value);
            }
            if (thisIndex >= dist)
                when passValue[dist](int stepDist, int val) serial {
                    value += val;
                }
        }
        serial { contribute(0, NULL, CkReduction::nop, cb); }
    })SRC";
    const auto tree = charmsharp::detail::parse_sdag_body(source);
    ++checks;
    if (!tree.has_sdag || tree.statements.size() != 2 ||
        tree.statements[0].kind != SdagKind::For ||
        tree.statements[1].kind != SdagKind::Serial)
        throw std::runtime_error("unexpected SDAG top-level structure");
    const auto& loop_body = tree.statements[0].children.at(0);
    ++checks;
    if (loop_body.kind != SdagKind::Block || loop_body.children.size() != 2 ||
        loop_body.children[0].kind != SdagKind::If ||
        loop_body.children[1].kind != SdagKind::If)
        throw std::runtime_error("unexpected SDAG nested control structure");
    const auto& receive = loop_body.children[1].children.at(0);
    ++checks;
    if (receive.kind != SdagKind::When || receive.children.size() != 1 ||
        receive.children[0].kind != SdagKind::Serial ||
        source.substr(receive.begin, 4) != "when")
        throw std::runtime_error("incorrect SDAG receive node or source range");

    const auto plain = charmsharp::detail::parse_sdag_body(R"SRC({
        const char* text = "when ghost() {}";
        // overlap { }
        doSomething();
    })SRC");
    ++checks;
    if (plain.has_sdag || plain.statements.size() != 2)
        throw std::runtime_error("ordinary C++ was misidentified as SDAG");

    const auto multiple = charmsharp::detail::parse_sdag_body(
        "{ when first(int x), second(int y) { use(x, y); } }");
    ++checks;
    if (!multiple.has_sdag || multiple.statements.size() != 1 ||
        multiple.statements[0].kind != SdagKind::When)
        throw std::runtime_error("multi-receive when not recognized");

    ++checks;
    try {
        (void)charmsharp::detail::parse_sdag_body("{ when receive(int x) x++; }");
        throw std::runtime_error("expected malformed when to be rejected");
    } catch (const charmsharp::detail::SdagError& error) {
        contains(error.what(), "expected SDAG receive body");
    }
    rejects("chare Worker { entry: void start() { when recv(int n) x++; } };",
            "expected SDAG receive body");
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
    rejects("chare A : public Base { entry: void f(); };", "unresolved or later-defined");
    rejects("chare A : public Base {}; chare Base {};", "defined before derived");
    rejects("chare Base {}; chare A : Base {};", "explicit public");
    rejects("chare Base {}; chare A : private Base {};", "explicit public");
    rejects("chare Base {}; chare A : protected Base {};", "explicit public");
    rejects("chare A {}; chare B {}; chare C : public A, public B {};", "multiple parallel");
    rejects("group Base {}; chare A : public Base {};", "only chare-to-chare");
    rejects("struct Base {}; chare A : virtual Base {};", "virtual base");
    rejects("struct Base {}; chare A : public virtual Base {};", "virtual base");
    rejects("struct Base {}; chare A : public Base<int> {};", "complex base");
    rejects("chare A : public {};", "expected a simple base class");
    rejects("chare A : {};", "expected a base class");
    rejects("chare A : public Later {}; struct Later {};", "later-defined");
    rejects("struct Base {}; chare A : Base, Base {};", "duplicate base class");
    rejects("struct Base {}; chare A : Base, {};", "after ','");
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
    rejects("message M : Base {};", "unresolved or later-defined");
    rejects("message A {}; message B : public A {};", "message-to-message");
    rejects("message B : public A {}; message A {};", "message-to-message");
    rejects("chare Worker {}; message M : Worker {};", "message inheritance from parallel");
    rejects("struct Base {}; message M : virtual Base {};", "virtual base");
    rejects("struct Base {}; message M : Base, Base {};", "duplicate base");
    rejects("struct Base {}; message M : Base<int> {};", "complex base");
    rejects("message M : public Later {}; struct Later {};", "later-defined");
    rejects("message M { int values[]; };", "variable-size message arrays");
    rejects("message M { entry: void foo(); };", "entry: is only valid");
    rejects("message M {}; message M {};", "duplicate");
    rejects("message M {}; chare M {};", "duplicate");
    rejects("namespace N { message M {}; }", "namespace messages");
    rejects("#ifdef X\nmessage M {};\n#endif", "preprocessor conditional");
    rejects("readonly int;", "expected readonly Type name");
    rejects("readonly int* count;", "expected readonly Type name");
    rejects("readonly unsigned int count;", "initializers, pointers and complex");
    rejects("readonly int count = 3;", "initializers, pointers and complex");
    rejects("readonly int values[];", "fixed numeric bound");
    rejects("readonly int values[0];", "positive decimal integer");
    rejects("readonly int values[2+1];", "fixed numeric bound");
    rejects("readonly int x; readonly double x;", "duplicate");
    rejects("namespace N { readonly int x; }", "global scope");
    rejects("#if 0\nreadonly int x;\n#endif", "preprocessor conditional");

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
    reduction_target_entries();
    entry_constructor_initializers();
    marshalled_entry_arrays();
    fixed_size_messages();
    readonly_globals();
    ordinary_inheritance();
    message_inheritance();
    parallel_inheritance();
    sdag_structure();
    diagnostics();
    std::cout << "PASS " << checks << " checks\n";
}

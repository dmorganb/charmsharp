#include "charmsharp/translator.hpp"
#include "../src/sdag.hpp"
#include "../src/lexer.hpp"
#include "../src/semantic.hpp"
#include "../src/ci_emitter.hpp"
#include "../src/cpp_emitter.hpp"

#include <algorithm>
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

void chare_template_declaration() {
    constexpr std::string_view source = R"SRC(template<typename T>
chare Worker {
entry:
    Worker(T initial) : value(initial) {}
    void process(T next) { value = next; }
private:
    T value;
};
)SRC";
    const auto result = charmsharp::translate(source, "TemplateModule");
    contains(result.interface_source, "template<typename T>\n    chare Worker {");
    contains(result.interface_source, "entry Worker(T initial);");
    contains(result.interface_source, "entry void process(T next);");
    contains(result.template_header_source, "template<typename T>\nclass Worker : public CBase_Worker<T>");
    contains(result.template_header_source, "T value;");
    contains(result.cpp_source, "#include \"TemplateModule_templates.h\"");
    rejects("template<typename T, typename U> chare Worker {};", "one typename/class parameter");
    rejects("template<int N> chare Worker {};", "one typename/class parameter");
    rejects("template<typename T> group Worker {};", "only scalar chare templates");

    // Ordinary C++ templates are not Charm# constructs.
    const auto ordinary = charmsharp::translate(
        "template<typename T> struct Plain { T item; }; chare Foo {};", "TemplateModule");
    contains(ordinary.cpp_source, "template<typename T> struct Plain { T item; };");
}

void explicit_chare_template_registration() {
    const auto generated = charmsharp::translate(R"SRC(
template<typename T>
chare Worker {
entry:
    Worker(T initial) {}
};
template chare Worker<int>;
chare Main {
entry:
    Main(CkArgMsg* msg) { delete msg; CProxy_Worker<int>::ckNew(42); }
};
)SRC", "TemplateReg");
    contains(generated.interface_source, "template<typename T>");
    contains(generated.interface_source, "extern module TemplateReg_specializations;");
    contains(generated.registration_interface_source, "chare Worker<int>;");
    contains(generated.registration_interface_source, "extern module TemplateReg;");
    contains(generated.template_header_source, "CBase_Worker<T>");
    contains(generated.cpp_source, "#include \"TemplateReg_specializations.decl.h\"");
    contains(generated.template_header_source, "#define CK_TEMPLATES_ONLY");
    ++checks;
    if (generated.cpp_source.find("template chare Worker<int>;") != std::string::npos)
        throw std::runtime_error("registration syntax leaked into generated C++");
    rejects("template chare Worker<int>;", "preceding template chare");
    rejects("template<typename T> chare Worker {}; template chare Worker<T>;",
            "concrete specialization");
    rejects("template<typename T> chare Worker {}; template chare Worker<int>; template chare Worker<int>;",
            "duplicate explicit chare template");
}

void inferred_chare_template_registration() {
    const auto source = R"SRC(
template<typename T> chare Worker {
entry:
    Worker(T x) {}
};
chare Main {
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        CProxy_Worker<int>::ckNew(1);
        CProxy_Worker< int >::ckNew(2);
        CProxy_Worker<double>::ckNew(3);
        const char* text = "CProxy_Worker<float>";
        // CProxy_Worker<long> should not be registered.
    }
};
)SRC";
    const auto generated = charmsharp::translate(source, "AutoReg");
    contains(generated.interface_source, "extern module AutoReg_specializations;");
    contains(generated.registration_interface_source, "chare Worker<int>;");
    contains(generated.registration_interface_source, "chare Worker<double>;");
    ++checks;
    if (generated.registration_interface_source.find("chare Worker<int>;",
            generated.registration_interface_source.find("chare Worker<int>;") + 1) != std::string::npos)
        throw std::runtime_error("duplicate inferred specialization");
    ++checks;
    if (generated.registration_interface_source.find("Worker<float>") != std::string::npos ||
        generated.registration_interface_source.find("Worker<long>") != std::string::npos)
        throw std::runtime_error("inferred registration from string or comment");
    contains(generated.cpp_source, "CProxy_Worker<int>::ckNew(1);");
    contains(generated.template_header_source, "#define CK_TEMPLATES_ONLY");

    const auto mixed = charmsharp::translate(R"SRC(
template<typename T> chare Worker { entry: Worker(T x) {} };
template chare Worker< int >;
chare Main { entry: Main(CkArgMsg* msg) {
    delete msg;
    CProxy_Worker<int>::ckNew(4);
    CProxy_Worker<T>::ckNew(5);
    CProxy_Worker<std::vector<int>>::ckNew({});
} };
)SRC", "MixedReg");
    contains(mixed.registration_interface_source, "chare Worker< int >;");
    ++checks;
    if (mixed.registration_interface_source.find("chare Worker<int>;") != std::string::npos ||
        mixed.registration_interface_source.find("Worker<T>") != std::string::npos ||
        mixed.registration_interface_source.find("Worker<std") != std::string::npos)
        throw std::runtime_error("duplicate or dependent specialization inferred");
    const auto no_use = charmsharp::translate(
        "template<typename T> chare Worker { entry: Worker(T x) {} };", "UnusedReg");
    equal(no_use.registration_interface_source, "");
    contains(no_use.template_header_source, "#define CK_TEMPLATES_ONLY");
}

void templated_mainchare_conflict() {
    rejects("template<typename T> chare Main { entry: Main(T x) {} };",
            "templated chare Main is not supported as an implicit mainchare");
    rejects("template<class T> chare Main { entry: Main(T x) {} };",
            "templated chare Main is not supported as an implicit mainchare");

    const auto ordinary = charmsharp::translate(
        "chare Main { entry: Main(CkArgMsg* args) { delete args; } };", "Implicit");
    contains(ordinary.interface_source, "mainchare Main {");

    const auto template_worker = charmsharp::translate(
        "template<typename T> chare Worker { entry: Worker(T x) {} };", "Template");
    contains(template_worker.interface_source, "template<typename T>");
    contains(template_worker.interface_source, "chare Worker {");
}

void template_header_order_and_inheritance_guards() {
    const auto output = charmsharp::translate(R"SRC(
chare Main { entry: Main(CkArgMsg* msg) { delete msg; } };
template<typename T> chare First { entry: First(T value) {} };
template<typename T> chare Second { entry: Second(T value) {} };
template chare First<int>;
template chare Second<double>;
)SRC", "Ordered");
    const auto first = output.template_header_source.find("class First : public CBase_First<T>");
    const auto second = output.template_header_source.find("class Second : public CBase_Second<T>");
    const auto support = output.template_header_source.find("#define CK_TEMPLATES_ONLY");
    ++checks;
    if (first == std::string::npos || second == std::string::npos ||
        support == std::string::npos || !(first < second && second < support))
        throw std::runtime_error("template definitions must precede generated template support");
    contains(output.cpp_source, "#include \"Ordered_templates.h\"");
    ++checks;
    if (output.cpp_source.find("#include \"Ordered_templates.h\"",
            output.cpp_source.find("#include \"Ordered_templates.h\"") + 1) != std::string::npos)
        throw std::runtime_error("duplicate generated template header include");
    contains(output.registration_interface_source, "chare First<int>;");
    contains(output.registration_interface_source, "chare Second<double>;");

    rejects(R"SRC(
struct Domain {};
template<typename T> chare Worker : public Domain { entry: Worker(T x) {} };
)SRC", "templated chare inheritance requires shared-header base resolution");
    rejects(R"SRC(
chare Base { entry: Base() {} };
template<typename T> chare Worker : public Base { entry: Worker(T x) {} };
)SRC", "templated chare inheritance requires shared-header base resolution");
}

void multiple_concrete_chare_template_registrations() {
    const auto result = charmsharp::translate(R"SRC(
template<typename T>
chare Worker { entry: Worker(T value) {} };
chare Main { entry: Main(CkArgMsg* msg) {
    CProxy_Worker<int>::ckNew(42);
    CProxy_Worker<double>::ckNew(3.5);
    CProxy_Worker<int>::ckNew(7);
} };
)SRC", "MultiTemplate");
    contains(result.registration_interface_source, "chare Worker<int>;");
    contains(result.registration_interface_source, "chare Worker<double>;");
    contains(result.interface_source, "extern module MultiTemplate_specializations;");
    contains(result.cpp_source, "CProxy_Worker<double>::ckNew(3.5);");
    contains(result.template_header_source, "class Worker : public CBase_Worker<T>");
    ++checks;
    const auto first = result.registration_interface_source.find("chare Worker<int>;");
    if (result.registration_interface_source.find("chare Worker<int>;", first + 1) !=
        std::string::npos)
        throw std::runtime_error("duplicate specialization of same type");
    ++checks;
    if (result.registration_interface_source.find("chare Worker<int>;") >
        result.registration_interface_source.find("chare Worker<double>;"))
        throw std::runtime_error("specialization registration order changed");
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

void preserves_numeric_literals_and_logical_lines() {
    for (const auto literal : {"1'000", "0xAB'CD", "0b1010'0011", "1.23'45e+6", "0x1.a'bp-2"}) {
        const auto tokens = charmsharp::detail::tokenize(literal);
        ++checks;
        if (tokens.size() != 1 || tokens.front().kind != charmsharp::detail::Kind::Number ||
            tokens.front().begin != 0 || tokens.front().end != std::string_view(literal).size())
            throw std::runtime_error("numeric literal was split");
        const auto source = std::string("chare A { entry: void f() { auto n = ") + literal + "; } };";
        contains(charmsharp::translate(source, "M").cpp_source,
                 std::string("auto n = ") + literal + ";");
    }
    contains(charmsharp::translate("chare A { entry: void f() { char c = 'x'; } };", "M").cpp_source,
             "char c = 'x';");
    for (const auto newline : {"\n", "\r\n"}) {
        const auto prefix = std::string("// continued \\") + newline +
            "chare Hidden {}; \\" + newline + "} still commented" + newline +
            "#define BRACES \\" + newline + "{ chare Macro {}; \\" + newline + "}" + newline;
        const auto source = prefix + "chare Visible {};";
        const auto output = charmsharp::translate(source, "M");
        equal(output.interface_source, "module M {\n    chare Visible {\n    };\n};\n");
        contains(output.cpp_source, prefix);
        const auto tokens = charmsharp::detail::tokenize(source);
        ++checks;
        if (tokens.front().kind != charmsharp::detail::Kind::Directive ||
            tokens.at(1).begin != prefix.size())
            throw std::runtime_error("logical line changed physical token offsets");
        try {
            (void)charmsharp::translate(prefix + "chare {", "M");
            throw std::runtime_error("expected invalid declaration diagnostic");
        } catch (const charmsharp::Diagnostic& error) {
            contains(charmsharp::format_diagnostic(prefix + "chare {", error), "7:1:");
        }
    }
    const auto eof = charmsharp::detail::tokenize("// continued \\\nchare Hidden {};");
    ++checks;
    if (!eof.empty()) throw std::runtime_error("continued comment leaked at EOF");
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

void implicit_mainchare_convention() {
    const auto implicit = charmsharp::translate(R"SRC(chare Main {
entry:
    Main(CkArgMsg* args) { delete args; CkExit(); }
};
chare Worker { entry: void ping() {} };
)SRC", "Implicit");
    contains(implicit.interface_source, "mainmodule Implicit {");
    contains(implicit.interface_source, "mainchare Main {");
    contains(implicit.interface_source, "chare Worker {");
    contains(implicit.cpp_source, "class Main : public CBase_Main");
    ++checks;
    const auto model = charmsharp::detail::analyze("chare Main { entry: Main(CkArgMsg* m); };", "I");
    if (!model.is_mainmodule || model.parallel_types.size() != 1 ||
        model.parallel_types[0].kind != charmsharp::detail::ParallelKind::Mainchare)
        throw std::runtime_error("implicit Main was not classified as a mainchare");

    const auto no_main = charmsharp::translate("chare Worker { entry: void ping(); };", "Library");
    contains(no_main.interface_source, "module Library {");
    ++checks;
    if (no_main.interface_source.find("mainmodule") != std::string::npos)
        throw std::runtime_error("ordinary chare unexpectedly created a mainmodule");

    for (const auto& source : {
        "array [1D] chare Main { entry: void ping(); };",
        "group Main { entry: void ping(); };",
        "nodegroup Main { entry: void ping(); };"}) {
        const auto output = charmsharp::translate(source, "NotMain");
        contains(output.interface_source, "module NotMain {");
        ++checks;
        if (output.interface_source.find("mainchare Main") != std::string::npos)
            throw std::runtime_error("non-scalar Main should not be a mainchare");
    }
    const auto explicitly_named = charmsharp::translate(
        "mainchare Startup { entry: Startup(CkArgMsg* msg) { delete msg; } };", "Explicit");
    contains(explicitly_named.interface_source, "mainmodule Explicit {");
    contains(explicitly_named.interface_source, "mainchare Startup {");

    rejects("chare Main {}; mainchare Startup {};", "only one mainchare");
    rejects("mainchare Startup {}; chare Main {};", "only one mainchare");
    rejects("mainchare Main {}; chare Main {};", "duplicate chare name");
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


void instrumentation_entry_attributes() {
    const auto output = charmsharp::translate(R"SRC(chare Worker {
entry:
    [[charm::notrace]] void ping() { CkPrintf("ping"); }
    [[charm::appwork]] void pong() { CkPrintf("pong"); }
    [[charm::notrace]] [[charm::appwork]] void both(int value) {
        CkPrintf("%d", value);
    }
};)SRC", "Instrumentation");
    contains(output.interface_source, "entry [notrace] void ping();");
    contains(output.interface_source, "entry [appwork] void pong();");
    contains(output.interface_source, "entry [notrace, appwork] void both(int value);");
    contains(output.cpp_source, "void both(int value)");
    ++checks;
    if (output.cpp_source.find("[[charm::") != std::string::npos)
        throw std::runtime_error("instrumentation attributes leaked into C++ output");
    rejects("chare Worker { entry: [[charm::notrace]] Worker(); };",
            "entry constructors cannot");
    rejects("chare Worker { entry: [[charm::notrace]] [[charm::notrace]] void ping(); };",
            "duplicate Charm++");
    rejects("chare Worker { entry: [[charm::notrace]] [[charm::threaded]] void ping(); };",
            "unsupported Charm++ entry attribute combination");
    rejects("chare Worker { entry: [[charm::appwork]] [[charm::sync]] void ping(); };",
            "unsupported Charm++ entry attribute combination");
}

void ownership_and_exclusive_entry_attributes() {
    const auto output = charmsharp::translate(R"SRC(message Data { int value; };
group Consumer {
entry:
    Consumer() {}
    [[charm::nokeep]] void consume(Data* data) { CkPrintf("%d", data->value); }
};
nodegroup Gate {
entry:
    Gate() {}
    [[charm::exclusive]] void enter() { CkPrintf("exclusive"); }
};)SRC", "Ownership");
    contains(output.interface_source, "entry [nokeep] void consume(Data* data);");
    contains(output.interface_source, "entry [exclusive] void enter();");
    contains(output.cpp_source, "void consume(Data* data)");
    contains(output.cpp_source, "void enter()");
    ++checks;
    if (output.cpp_source.find("[[charm::") != std::string::npos)
        throw std::runtime_error("ownership attributes leaked into C++ output");
    rejects("chare A { entry: [[charm::exclusive]] void f(); };",
            "exclusive entry attribute requires a nodegroup");
    rejects("group A { entry: [[charm::exclusive]] void f(); };",
            "exclusive entry attribute requires a nodegroup");
    rejects("array [1D] chare A { entry: [[charm::exclusive]] void f(); };",
            "exclusive entry attribute requires a nodegroup");
    rejects("group A { entry: [[charm::nokeep]] A(); };",
            "entry constructors cannot");
    rejects("group A { entry: [[charm::nokeep]] [[charm::nokeep]] void f(); };",
            "duplicate Charm++");
    rejects("nodegroup A { entry: [[charm::exclusive]] [[charm::nokeep]] void f(); };",
            "unsupported Charm++ entry attribute combination");
}

void direct_call_entry_attributes() {
    const auto output = charmsharp::translate(R"SRC(group Service {
entry:
    Service() {}
    [[charm::local]] int twice(int value) { return 2 * value; }
    [[charm::inline]] void ping(int x) { CkPrintf("%d", x); }
};
array [1D] chare Cell {
entry:
    [[charm::local]] int index() { return thisIndex; }
};)SRC", "Direct");
    contains(output.interface_source, "entry [local] int twice(int value);");
    contains(output.interface_source, "entry [inline] void ping(int x);");
    contains(output.interface_source, "entry [local] int index();");
    contains(output.cpp_source, "int twice(int value) { return 2 * value; }");
    ++checks;
    if (output.cpp_source.find("[[charm::") != std::string::npos)
        throw std::runtime_error("direct-call attributes leaked into C++ output");
    rejects("chare A { entry: [[charm::local]] int f(); };",
            "local entry attribute requires a group or chare array");
    rejects("mainchare A { entry: [[charm::local]] void f(); };",
            "local entry attribute requires a group or chare array");
    rejects("nodegroup A { entry: [[charm::local]] void f(); };",
            "local entry attribute requires a group or chare array");
    rejects("chare A { entry: [[charm::inline]] int f(); };",
            "entry methods must return void");
    rejects("group A { entry: [[charm::local]] [[charm::inline]] int f(); };",
            "unsupported Charm++ entry attribute combination");
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

void sdag_array_edits_do_not_overlap() {
    for (const auto source : {
        "chare A { entry: void f(int n, float data[n]) { when ping() {} }void after() {} };",
        "chare A { entry: void recv(int n, float data[n]);void after() {} void f() { when recv(int n, float data[n]) {} } };"
    }) {
        const auto generated = charmsharp::translate(source, "M");
        const auto expected = std::string("#include \"M.decl.h\"\n") +
            "class A : public CBase_A { public: void after() {} " +
            (std::string_view(source).find("void recv") != std::string_view::npos ? " " : "") +
            "\npublic:\n    A_SDAG_CODE\n};\n#include \"M.def.h\"\n";
        equal(generated.cpp_source, expected);
        contains(generated.interface_source, "float data[n]");
    }
    const auto source = "chare A { entry: void f(int n, float data[n]); };";
    auto model = charmsharp::detail::analyze(source, "M");
    model.marshalled_arrays.push_back(model.marshalled_arrays.front());
    ++checks;
    try {
        (void)charmsharp::detail::emit_cpp(source, model);
    } catch (const std::logic_error& error) {
        contains(error.what(), "overlapping or invalid");
        return;
    }
    throw std::runtime_error("overlapping edits were accepted");
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

void message_methods_preserve_ordinary_cpp() {
    const auto body = R"CPP({
public:
    int values[2]{};
    static constexpr auto answer = []() { return 42; };
    int operator[](int i) const { return values[i]; }
    void accept(int items[]);
    int calculate() {
        auto helper = []() { return 42; };
        int local[] = {1, 2};
        return helper() + local[0];
    }
};)CPP";
    const auto source = std::string("message Payload ") + body;
    const auto generated = charmsharp::translate(source, "M");
    equal(generated.cpp_source, std::string("#include \"M.decl.h\"\nclass Payload : public CMessage_Payload ") +
          body + "\n#include \"M.def.h\"\n");
    equal(generated.interface_source, "module M {\n    message Payload;\n};\n");
    rejects("message M { int* values[]; };", "standalone primitive");
    rejects("message M { int value, values[]; };", "standalone primitive");
    rejects("message M { entry: void f(); };", "entry: is only valid");
}

void variable_size_message_arrays() {
    const auto result = charmsharp::translate(R"SRC(message DataMessage {
    int count;
private:
    double values[];
public:
    int indices[];
    int fixed[4];
    int sum() const { return count + indices[0] + fixed[0]; }
};
)SRC", "VarMsg");
    contains(result.interface_source, "message DataMessage {\n        double values[];\n        int indices[];\n    };");
    contains(result.cpp_source, "friend class CMessage_DataMessage;");
    contains(result.cpp_source, "double *values;");
    contains(result.cpp_source, "int *indices;");
    contains(result.cpp_source, "int count;");
    contains(result.cpp_source, "int fixed[4];");
    contains(result.cpp_source, "int sum() const { return count + indices[0] + fixed[0]; }");
    const auto single = charmsharp::translate(
        "message One { public: float numbers[]; };", "OneMsg");
    contains(single.interface_source, "float numbers[];");
    contains(single.cpp_source, "float *numbers;");
    rejects("message Bad { float* numbers[]; };", "standalone primitive");
    rejects("message Bad { std::string numbers[]; };", "standalone primitive");
    rejects("message Bad { int first[], second[]; };", "standalone primitive");
    rejects("message Bad { double numbers[] = {}; };", "standalone primitive");
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

void pupable_class_registration() {
    const auto output = charmsharp::translate(R"SRC(
[[charm::pupable]]
class State : public PUP::able {
    int value = 42;
public:
    State(CkMigrateMessage* m) : PUP::able(m) {}
    void pup(PUP::er& p) override { PUP::able::pup(p); p | value; }
};
chare Main { entry: Main(CkArgMsg* args) { delete args; CkExit(); } };
)SRC", "Pupable");
    contains(output.interface_source, "PUPable State;");
    contains(output.cpp_source, "class State : public PUP::able");
    contains(output.cpp_source, "PUPable_decl(State);");
    contains(output.cpp_source, "void pup(PUP::er& p) override");
    ++checks;
    if (output.cpp_source.find("[[charm::pupable]]") != std::string::npos)
        throw std::runtime_error("PUPable attribute leaked into C++");
    rejects("[[charm::pupable]] struct S : public PUP::able { };",
            "non-template class");
    rejects("[[charm::pupable]] class S { };",
            "direct public");
    rejects("[[charm::pupable]] class S : private PUP::able { };",
            "direct public");
    rejects("[[charm::pupable]] class S : public PUP::able { void pup(PUP::er&); };",
            "migration constructor");
    rejects("[[charm::pupable]] class S : public PUP::able { S(CkMigrateMessage*); };",
            "pup(PUP::er&)");
    rejects("namespace N { [[charm::pupable]] class S {}; }",
            "global scope");
    rejects("#ifdef X\n[[charm::pupable]] class S {};\n#endif",
            "preprocessor conditionals");
}

void init_hook_attributes() {
    const auto result = charmsharp::translate(R"SRC(
[[charm::initnode]]
void initializeNode() { CkPrintf("Node\\n"); }
[[charm::initproc]]
void initializeProcessor(void) { CkPrintf("PE\\n"); }
)SRC", "InitHooks");
    contains(result.interface_source, "initnode void initializeNode();");
    contains(result.interface_source, "initproc void initializeProcessor();");
    contains(result.cpp_source, "void initializeNode() {");
    contains(result.cpp_source, "void initializeProcessor(void) {");
    ++checks;
    if (result.cpp_source.find("[[charm::") != std::string::npos)
        throw std::runtime_error("init hook attribute leaked into C++ output");
    rejects("[[charm::initnode]] int bad() {}", "top-level void");
    rejects("[[charm::initproc]] void bad(int x) {}", "no parameters");
    rejects("[[charm::initproc]] void bad();", "function body");
    rejects("[[charm::initnode]] void f() {} [[charm::initproc]] void f() {}",
            "duplicate Charm# init hook");
    rejects("namespace N { [[charm::initproc]] void f() {} }", "top-level");
    rejects("[[charm::initnode]] void f() {} [[charm::initproc, other]] void g() {}",
            "expected [[charm");
}

void readonly_message_pointers() {
    const auto result = charmsharp::translate(R"SRC(message Settings {
public:
    int count = 0;
    int data[];
};
readonly Settings* settings;
chare Main {
entry:
    Main(CkArgMsg* args) { delete args; settings = new (3) Settings; }
};
)SRC", "ReadonlyMsg");
    contains(result.interface_source, "message Settings {");
    contains(result.interface_source, "readonly message Settings* settings;");
    contains(result.cpp_source, "Settings* settings;");
    ++checks;
    if (result.cpp_source.find("readonly Settings") != std::string::npos)
        throw std::runtime_error("readonly qualifier leaked into C++");
    rejects("readonly int* count;", "preceding Charm# message");
    rejects("readonly Settings* x; message Settings {};", "preceding Charm# message");
    rejects("message Settings {}; readonly Settings** x;", "expected readonly");
    rejects("message Settings {}; readonly Settings* x[2];", "cannot be arrays");
    rejects("message Settings {}; readonly Settings* x = nullptr;", "initializers");
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


void semantic_model_is_populated_without_changing_translation() {
    using charmsharp::detail::ParallelKind;
    constexpr std::string_view source = R"SRC(readonly int total[4];
message Result { int value; };
mainchare Main {
entry:
    Main(CkArgMsg* message) { delete message; }
};
array [2D] chare Worker {
    int state = 0;
entry:
    [[charm::threaded]] void step(int n);
    void start() {
        when recv(int x) { state = x; }
    }
};)SRC";

    const auto model = charmsharp::detail::analyze(source, "SemanticTest");
    equal(model.module_name, "SemanticTest");
    ++checks;
    if (!model.is_mainmodule || model.parallel_types.size() != 2 ||
        model.messages.size() != 1 || model.readonly_variables.size() != 1)
        throw std::runtime_error("semantic model lost top-level declarations");

    const auto& readonly = model.readonly_variables.front();
    equal(readonly.type_name, "int");
    equal(readonly.name, "total");
    ++checks;
    if (!readonly.array_bound || *readonly.array_bound != "4" ||
        source.substr(readonly.source.begin, 8) != "readonly")
        throw std::runtime_error("readonly metadata or source range missing");

    const auto& message = model.messages.front();
    equal(message.name, "Result");
    ++checks;
    if (source.substr(message.source.begin, 7) != "message")
        throw std::runtime_error("message source range missing");

    const auto& main = model.parallel_types.front();
    ++checks;
    if (main.kind != ParallelKind::Mainchare || main.entries.size() != 1 ||
        main.entries.front().name != "Main" ||
        !main.entries.front().return_type.empty())
        throw std::runtime_error("mainchare constructor metadata missing");

    const auto& worker = model.parallel_types.back();
    equal(worker.name, "Worker");
    equal(worker.array_dimension, "2");
    ++checks;
    if (worker.kind != ParallelKind::Array || worker.entries.size() != 3 ||
        source.substr(worker.source.begin, 5) != "array")
        throw std::runtime_error("array chare metadata missing");

    const auto& step = worker.entries.at(0);
    equal(step.name, "step");
    equal(step.return_type, "void");
    equal(step.parameter_list, "(int n)");
    ++checks;
    if (step.is_definition || step.is_sdag || step.attributes.size() != 1 ||
        step.attributes.front() != "threaded")
        throw std::runtime_error("annotated entry metadata missing");

    const auto& start = worker.entries.at(1);
    ++checks;
    if (!start.is_definition || !start.is_sdag || !start.sdag_body ||
        !start.sdag_body->has_sdag || start.sdag_body->statements.empty() ||
        source.substr(start.sdag_body_start, 1) != "{")
        throw std::runtime_error("SDAG structural representation missing");

    const auto& recv = worker.entries.at(2);
    equal(recv.name, "recv");
    equal(recv.parameter_list, "(int x)");
    ++checks;
    if (!recv.is_receive || !recv.is_implicit)
        throw std::runtime_error("implicit SDAG receive metadata missing");

    // analyze() does not emit; the public translator delegates .ci rendering
    // to the same model-backed emitter.
    const auto output = charmsharp::translate(source, "SemanticTest");
    const auto emitted = charmsharp::detail::emit_ci(source, model);
    equal(emitted, output.interface_source);
    equal(emitted, charmsharp::detail::emit_ci(source, model));
    equal(charmsharp::detail::emit_cpp(source, model), output.cpp_source);
    equal(charmsharp::detail::emit_cpp(source, model),
          charmsharp::detail::emit_cpp(source, model));
    auto renamed_model = model;
    renamed_model.module_name = "RenamedModule";
    contains(charmsharp::detail::emit_ci(source, renamed_model),
             "mainmodule RenamedModule {");
    contains(charmsharp::detail::emit_cpp(source, renamed_model),
             "#include \"RenamedModule.decl.h\"");
    contains(output.interface_source, "entry void recv(int x);");
    contains(output.cpp_source, "Worker_SDAG_CODE");

    const auto explicit_recv = charmsharp::detail::analyze(R"SRC(chare Worker {
entry:
    void recv(int x);
    void start() { when recv(int x) { CkExit(); } }
};)SRC", "ExplicitReceive");
    ++checks;
    if (explicit_recv.parallel_types.size() != 1 ||
        explicit_recv.parallel_types.front().entries.size() != 2 ||
        !explicit_recv.parallel_types.front().entries.front().is_receive ||
        explicit_recv.parallel_types.front().entries.front().is_implicit)
        throw std::runtime_error("explicit SDAG receive was duplicated");
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


void basic_sdag_when_lowering() {
    const auto implicit = charmsharp::translate(R"SRC(mainchare Main {
entry:
    Main(CkArgMsg* m) { delete m; }
};
chare Worker {
    int last = 0;
entry:
    Worker() {}
    void start() {
        when deliver(int value) {
            last = value;
            CkExit();
        }
    }
};)SRC", "BasicWhen");
    contains(implicit.interface_source, "entry void deliver(int value);");
    contains(implicit.interface_source, "entry void start() {");
    contains(implicit.interface_source, "when deliver(int value) serial {");
    contains(implicit.interface_source, "last = value;");
    contains(implicit.cpp_source, "Worker_SDAG_CODE");
    ++checks;
    if (implicit.cpp_source.find("void start() {") != std::string::npos)
        throw std::runtime_error("SDAG entry body leaked into C++");

    const auto explicit_entry = charmsharp::translate(R"SRC(chare Worker {
entry:
    void deliver(int value);
    void start() {
        when deliver(int value) serial { CkExit(); }
    }
};)SRC", "ExplicitWhen");
    contains(explicit_entry.interface_source, "entry void deliver(int value);");
    contains(explicit_entry.interface_source, "when deliver(int value) serial {");
    ++checks;
    if (explicit_entry.cpp_source.find("void deliver(int value);") != std::string::npos)
        throw std::runtime_error("explicit receive declaration leaked into C++");

    rejects("chare A { entry: void recv(int value); void start() { when recv(float value) { CkExit(); } } };",
            "incompatible explicit SDAG receive");
    rejects("chare A { entry: void recv(int value) {} void start() { when recv(int value) { CkExit(); } } };",
            "SDAG receive entry cannot have a C++ implementation");
    rejects("chare A { entry: void start() { when a(int n), b(int m) { CkExit(); } } };",
            "multiple receives");

}

void normalized_entry_signatures() {
    const auto source = R"SRC(chare A {
entry:
    void /* return comment */ recv(int value);
    void f() {
        when recv( int /* type comment */ other ) {}
        when recv(int third) {}
    }
};)SRC";
    const auto model = charmsharp::detail::analyze(source, "M");
    ++checks;
    if (model.parallel_types.front().entries.size() != 2 ||
        !model.parallel_types.front().entries.front().is_receive)
        throw std::runtime_error("compatible receive was not matched");
    const auto generated = charmsharp::translate(source, "M");
    contains(generated.interface_source, "entry void /* return comment */ recv(int value);");
    contains(generated.interface_source, "when recv( int /* type comment */ other ) serial {}");
    const auto overloaded = charmsharp::detail::analyze(
        "chare A { entry: void recv(float value); void recv(int value); "
        "void f() { when recv(int other) {} } };", "M");
    ++checks;
    if (overloaded.parallel_types.front().entries.front().is_receive ||
        !overloaded.parallel_types.front().entries.at(1).is_receive)
        throw std::runtime_error("wrong overload selected for receive");
    const auto arrays = charmsharp::detail::analyze(
        "chare A { entry: void recv(int n, float data[n]); "
        "void f() { when recv(int count, float values[count]) {} } };", "M");
    ++checks;
    if (!arrays.parallel_types.front().entries.front().is_receive)
        throw std::runtime_error("renamed array bound did not match");
    contains(charmsharp::translate(
        "chare A { entry: void f(const int value); void f(int* value); "
        "void f(unsigned long); void f(struct First); void f(struct Second); "
        "void f(const Widget); };", "M").interface_source,
        "entry void f(const Widget);");
    rejects("chare A { entry: void f(int x); void f( int /*comment*/ y ); };",
            "duplicate entry method signature");
    rejects("chare A { entry: void f(void); void f(); };", "duplicate entry method signature");
    rejects("chare A { entry: void f(unsigned long x); void f(unsigned long y); };",
            "duplicate entry method signature");
    rejects("chare A { entry: void f() { when ping() {} } void f() { when ping() {} } };",
            "duplicate entry method signature");
    rejects("chare A { entry: void f() { when ping() {} } void f(); };",
            "duplicate entry method signature");
    rejects("chare A { entry: void f(); void f() { when ping() {} } };",
            "duplicate entry method signature");
    rejects("chare A { entry: [[charm::threaded]] void recv(int x); "
            "void f() { when recv(int y) {} } };", "incompatible explicit SDAG receive");
    rejects("chare A { entry: void recv(int x) {} "
            "void f() { when recv( int y ) {} } };", "cannot have a C++ implementation");
    rejects("chare A { entry: void f() { when recv(int x) {} when recv(float y) {} } };",
            "inconsistent SDAG receive signatures");
}

void implicit_sdag_serial_runs() {
    const auto generated = charmsharp::translate(R"SRC(chare Worker {
    int total = 0;
entry:
    void start() {
        total = 0;
        total += 1;
        when first(int a) { total += a; }
        total += 2;
        when second(int b) serial { total += b; }
        report(total);
    }
};)SRC", "Sequence");
    contains(generated.interface_source, "entry void first(int a);");
    contains(generated.interface_source, "entry void second(int b);");
    contains(generated.interface_source, "serial {");
    contains(generated.interface_source, "total = 0;");
    contains(generated.interface_source, "total += 1;");
    contains(generated.interface_source, "when first(int a) serial {");
    contains(generated.interface_source, "when second(int b) serial {");
    contains(generated.interface_source, "report(total);");
    contains(generated.cpp_source, "Worker_SDAG_CODE");
    ++checks;
    if (generated.cpp_source.find("void start() {") != std::string::npos)
        throw std::runtime_error("segmented SDAG entry body leaked into C++");

    rejects("chare A { entry: void f() { int temp = 1; when ping() { CkExit(); } } };",
            "local declaration before an SDAG wait");
    rejects("chare A { entry: void f() { const int temp = 1; when ping() { CkExit(); } } };",
            "local declaration before an SDAG wait");
    rejects("chare A { entry: void f() { when ping(int n), pong(int m) {} } };",
            "multiple receives");
}

void sdag_checks_each_local_declaration() {
    for (const auto run : {
        "int x = 1; ping(); pong();",
        "ping(); int x = 1; pong();",
        "ping(); pong(); int x = 1;"
    }) {
        const auto body = std::string(run) + " when recv() { use(x); }";
        rejects("chare A { entry: void f() { " + body + " } };",
                "local declaration before an SDAG wait");
        rejects("chare A { entry: void f() { if (ready) { " + body + " } } };",
                "local declaration before an SDAG wait");
        rejects("chare A { entry: void f() { while (ready) { " + body + " } } };",
                "local declaration before an SDAG wait");
    }
    for (const auto declaration : {"auto x = 1;", "const int x = 1;", "unsigned x = 1;"})
        rejects(std::string("chare A { entry: void f() { ping(); ") + declaration +
                " when recv() { use(x); } } };", "local declaration before an SDAG wait");
    const auto persistent = charmsharp::translate(
        "chare A { int x; entry: void f() { ping(); x = 1; when recv() { use(x); } } };", "M");
    contains(persistent.interface_source, "ping(); x = 1;");
    contains(persistent.interface_source, "when recv() serial { use(x); }");
    // A local consumed in the final serial run does not cross a later wait.
    contains(charmsharp::translate(
        "chare A { entry: void f() { when recv() {} ping(); int x = 1; use(x); } };", "M").interface_source,
        "ping(); int x = 1; use(x);");
    const std::string located = "chare A { entry: void f() {\n  ping();\n  int x = 1;\n  when recv() {}\n} };";
    try {
        (void)charmsharp::translate(located, "M");
        throw std::runtime_error("expected local lifetime diagnostic");
    } catch (const charmsharp::Diagnostic& error) {
        contains(charmsharp::format_diagnostic(located, error), "3:3:");
    }
}

void sdag_if_for_ref_matching() {
    const auto generated = charmsharp::translate(R"SRC(chare Worker {
    int total = 0;
    int step = 0;
entry:
    void start() {
        for (step = 0; step < 2; ++step) {
            when deliver[step](int tag, int value) {
                total = total * 10 + value;
            }
        }
        if (total == 24) {
            finish(total);
        } else {
            finish(-1);
        }
    }
};)SRC", "Control");
    contains(generated.interface_source, "entry void deliver(int tag, int value);");
    contains(generated.interface_source, "when deliver[step](int tag, int value) serial {");
    contains(generated.interface_source, "for (step = 0; step < 2; ++step)");
    contains(generated.interface_source, "if (total == 24)");
    contains(generated.interface_source, "finish(-1);");
    contains(generated.cpp_source, "Worker_SDAG_CODE");

    const auto duplicate = charmsharp::translate(R"SRC(chare Worker {
entry:
    void start() {
        when deliver[0](int tag, int value) { CkPrintf("%d", value); }
        when deliver[1](int tag, int value) { CkExit(); }
    }
};)SRC", "Repeated");
    ++checks;
    const auto first = duplicate.interface_source.find("entry void deliver(int tag, int value);");
    if (first == std::string::npos ||
        duplicate.interface_source.find("entry void deliver(int tag, int value);", first + 1)
            != std::string::npos)
        throw std::runtime_error("repeated reference-matched receive registered more than once");

    rejects("chare A { entry: void start() { for (int step = 0; step < 2; ++step) { when deliver[step](int tag) {} } } };",
            "SDAG for index must be a persistent chare field");
    rejects("chare A { entry: void start() { when deliver(int x), other(int y) {} } };",
            "multiple receives");
}

void sdag_extended_controls() {
    const auto generated = charmsharp::translate(R"SRC(chare Worker {
    int iteration;
entry:
    void start() {
        while (iteration < 2) {
            when tick[iteration](int tag) {
                ++iteration;
            }
        }
        overlap {
            when left(int x) { CkPrintf("L%d", x); }
            when right(int y) { CkPrintf("R%d", y); }
        }
        case {
            when first(int x) { CkPrintf("A%d", x); }
            when second(int y) { CkPrintf("B%d", y); }
        }
        forall [iteration] (0:1, 1) {
            when indexed[iteration](int index) { CkPrintf("%d", index); }
        }
    }
};)SRC", "Extended");
    contains(generated.interface_source, "while (iteration < 2)");
    contains(generated.interface_source, "overlap {");
    contains(generated.interface_source, "case {");
    contains(generated.interface_source, "forall [iteration] (0:1, 1)");
    contains(generated.interface_source, "when tick[iteration](int tag) serial {");
    contains(generated.interface_source, "when left(int x) serial {");
    contains(generated.interface_source, "when right(int y) serial {");
    contains(generated.interface_source, "when first(int x) serial {");
    contains(generated.interface_source, "when second(int y) serial {");
    contains(generated.interface_source, "when indexed[iteration](int index) serial {");
    contains(generated.cpp_source, "Worker_SDAG_CODE");
    rejects("chare A { entry: void start() { forall [int i] (0:2, 1) { when value(int n) {} } } };",
            "SDAG forall index must name a persistent chare field");
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
    rejects("message M { int* values[]; };", "standalone primitive");
    rejects("message M { entry: void foo(); };", "entry: is only valid");
    rejects("message M {}; message M {};", "duplicate");
    rejects("message M {}; chare M {};", "duplicate");
    rejects("namespace N { message M {}; }", "namespace messages");
    rejects("#ifdef X\nmessage M {};\n#endif", "preprocessor conditional");
    rejects("readonly int;", "expected readonly Type name");
    rejects("readonly int* count;", "preceding Charm# message");
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


void sdag_ci_statement_source_locations() {
    constexpr std::string_view source = R"SRC(chare Worker {
entry:
    Worker() {}
    void start() {
        prepare();
        overlap {
            when first(int x) {
                consume_first(x);
            }
            when second(int y) {
                consume_second(y);
            }
        }
        finalize();
    }
};
)SRC";
    const auto generated = charmsharp::translate_many(
        {source}, "SDAGOrigins", {"Worker.csh"});
    const auto check = [&](std::string_view statement) {
        const auto generated_pos = generated.interface_source.find(statement);
        const auto source_pos = source.find(statement);
        if (generated_pos == std::string::npos || source_pos == std::string_view::npos)
            throw std::runtime_error("missing SDAG origin fixture statement");
        const auto generated_line = 1 + static_cast<std::size_t>(std::count(
            generated.interface_source.begin(),
            generated.interface_source.begin() + generated_pos, '\n'));
        const auto source_line = 1 + static_cast<std::size_t>(std::count(
            source.begin(), source.begin() + source_pos, '\n'));
        const auto origin = std::find_if(generated.interface_locations.begin(),
            generated.interface_locations.end(),
            [&](const charmsharp::CiSourceLocation& location) {
                return location.generated_line == generated_line;
            });
        ++checks;
        if (origin == generated.interface_locations.end() ||
            origin->source_line != source_line || origin->source_path != "Worker.csh") {
            std::cerr << "incorrect SDAG mapping for " << statement
                      << ": expected Worker.csh:" << source_line << '\n';
            std::exit(1);
        }
    };
    check("prepare();");
    check("when first(int x)");
    check("consume_first(x);");
    check("when second(int y)");
    check("consume_second(y);");
    check("finalize();");
}

} // namespace

int main() {
    sdag_ci_statement_source_locations();
    golden_counter();
    chare_template_declaration();
    explicit_chare_template_registration();
    inferred_chare_template_registration();
    multiple_concrete_chare_template_registrations();
    template_header_order_and_inheritance_guards();
    templated_mainchare_conflict();
    preserves_ordinary_cpp_and_literal_braces();
    preserves_numeric_literals_and_logical_lines();
    multiple_sections_and_definitions();
    multiple_chares_and_mainchare();
    implicit_mainchare_convention();
    groups_and_arrays();
    attributed_entries();
    instrumentation_entry_attributes();
    ownership_and_exclusive_entry_attributes();
    direct_call_entry_attributes();
    reduction_target_entries();
    entry_constructor_initializers();
    marshalled_entry_arrays();
    sdag_array_edits_do_not_overlap();
    fixed_size_messages();
    message_methods_preserve_ordinary_cpp();
    variable_size_message_arrays();
    readonly_globals();
    readonly_message_pointers();
    init_hook_attributes();
    pupable_class_registration();
    ordinary_inheritance();
    message_inheritance();
    parallel_inheritance();
    semantic_model_is_populated_without_changing_translation();
    sdag_structure();
    basic_sdag_when_lowering();
    normalized_entry_signatures();
    implicit_sdag_serial_runs();
    sdag_checks_each_local_declaration();
    sdag_if_for_ref_matching();
    sdag_extended_controls();
    diagnostics();
    std::cout << "PASS " << checks << " checks\n";
}


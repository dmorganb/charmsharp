#include "../lexer.hpp"
#include <cassert>
#include <stdexcept>

using charm_metrics::Category;
using charm_metrics::Language;
using charm_metrics::lex;

int main() {
    auto cpp = lex("#include <vector>\nint x = 1.0e-3; // ignored\nobj->method(x);", Language::Cpp);
    assert(cpp[0].spelling == "int" && cpp[0].category == Category::Operator);
    assert(cpp[1].spelling == "x" && cpp[1].category == Category::Operand);
    assert(cpp[3].spelling == "1.0e-3" && cpp[3].category == Category::Operand);
    assert(cpp[5].spelling == "obj" && cpp[6].spelling == "->");
    auto ci = lex("mainmodule Test { mainchare Main { entry Main(CkArgMsg* msg); }; };",
                  Language::CharmInterface);
    assert(ci[0].spelling == "mainmodule" && ci[0].category == Category::Operator);
    assert(ci[6].spelling == "entry" && ci[6].category == Category::Operator);
    auto cs = lex("mainchare Main { entry: Main(float data[n]); };", Language::CharmSharp);
    assert(cs[0].category == Category::Operator);
    assert(cs[3].spelling == "entry" && cs[3].category == Category::Operator);
    assert(lex("R\"tag(// untouched)tag\"", Language::Cpp).size() == 1);
    assert(lex("a::b -> c <<= 1", Language::Cpp)[1].spelling == "::");
    assert(lex("L\"hello\"", Language::Cpp).size() == 1);
    bool threw = false;
    try { (void)lex("/* unclosed", Language::Cpp); }
    catch (const std::runtime_error&) { threw = true; }
    assert(threw);
}

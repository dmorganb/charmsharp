#include "../metrics.hpp"

#include <cassert>
#include <cmath>

int main() {
    using namespace charm_metrics;
    Counts counts;
    counts.add(lex("int a = 1; a = a + 2;", Language::Cpp));
    const auto m = calculate(counts);
    assert(m.n1 == counts.operators.size());
    assert(m.n2 == counts.operands.size());
    assert(m.N1 + m.N2 == m.length);
    assert(m.n1 + m.n2 == m.vocabulary);
    assert(std::abs(m.volume - m.length * std::log2(static_cast<double>(m.vocabulary))) < 1e-9);
    assert(std::abs(m.difficulty - (m.n1 / 2.0) * (static_cast<double>(m.N2) / m.n2)) < 1e-9);
    assert(std::abs(m.effort - m.volume * m.difficulty) < 1e-9);

    Counts empty;
    const auto zero = calculate(empty);
    assert(zero.length == 0 && zero.volume == 0 && zero.difficulty == 0);
    Counts merged;
    merged.add(lex("x + x", Language::Cpp));
    merged.add(lex("x - y", Language::Cpp));
    assert(merged.operators.size() == 2);
    assert(merged.operands.at("x") == 3);
    assert(merged.operands.at("y") == 1);
    auto sample = lex("int n = 1; fn(n); n += 2;", Language::Cpp);
    Counts inclusive, no_punctuation;
    inclusive.add(sample);
    no_punctuation.add(sample, false);
    assert(inclusive.operators.at(";") == 3);
    assert(!no_punctuation.operators.contains(";"));
    assert(no_punctuation.operators.contains("="));
    assert(no_punctuation.operators.contains("+="));
    assert(inclusive.operands == no_punctuation.operands);
    assert(calculate(no_punctuation).N1 < calculate(inclusive).N1);

}

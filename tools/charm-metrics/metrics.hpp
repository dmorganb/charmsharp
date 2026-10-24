#pragma once
#include "lexer.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace charm_metrics {
struct Counts {
    std::map<std::string, std::uint64_t> operators;
    std::map<std::string, std::uint64_t> operands;
    // A sensitivity variant excludes only structural punctuation, not semantic operators.
    static bool structural_punctuation(std::string_view token) {
        return token == ";" || token == "," || token == "(" || token == ")" ||
               token == "{" || token == "}" || token == "[" || token == "]" ||
               token == ":";
    }
    void add(const std::vector<Token>& tokens, bool include_punctuation = true) {
        for (const auto& token : tokens) {
            if (!include_punctuation && token.category == Category::Operator &&
                structural_punctuation(token.spelling)) continue;
            ++(token.category == Category::Operator ? operators : operands)[token.spelling];
        }
    }
    void merge(const Counts& other) {
        for (const auto& [name, count] : other.operators) operators[name] += count;
        for (const auto& [name, count] : other.operands) operands[name] += count;
    }
};

struct Metrics {
    std::uint64_t n1 = 0, n2 = 0, N1 = 0, N2 = 0;
    std::uint64_t vocabulary = 0, length = 0;
    double volume = 0, difficulty = 0, effort = 0;
    double time_seconds = 0, estimated_bugs = 0;
};

inline Metrics calculate(const Counts& counts) {
    Metrics result;
    result.n1 = counts.operators.size();
    result.n2 = counts.operands.size();
    for (const auto& [_, count] : counts.operators) result.N1 += count;
    for (const auto& [_, count] : counts.operands) result.N2 += count;
    result.vocabulary = result.n1 + result.n2;
    result.length = result.N1 + result.N2;
    if (result.vocabulary > 1)
        result.volume = static_cast<double>(result.length) * std::log2(static_cast<double>(result.vocabulary));
    if (result.n2 != 0)
        result.difficulty = (static_cast<double>(result.n1) / 2.0) *
                            (static_cast<double>(result.N2) / static_cast<double>(result.n2));
    result.effort = result.volume * result.difficulty;
    result.time_seconds = result.effort / 18.0;
    // Predictive only: not a measured number of defects.
    result.estimated_bugs = std::pow(result.effort, 2.0 / 3.0) / 3000.0;
    return result;
}
} // namespace charm_metrics

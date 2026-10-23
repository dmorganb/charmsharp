#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace charmsharp::detail {

enum class Kind { Identifier, Number, Literal, Punctuation, Directive };

struct Token {
    Kind kind;
    std::size_t begin;
    std::size_t end;
};

[[nodiscard]] std::vector<Token> tokenize(std::string_view source);
[[nodiscard]] bool is_identifier_start(char ch);
[[nodiscard]] bool is_identifier_continue(char ch);

} // namespace charmsharp::detail

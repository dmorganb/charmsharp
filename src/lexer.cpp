#include "lexer.hpp"
#include "charmsharp/translator.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace charmsharp::detail {

bool is_identifier_start(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

bool is_identifier_continue(char ch) {
    return is_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

namespace {

bool starts_with_at(std::string_view source, std::size_t at, std::string_view text) {
    return source.substr(at, text.size()) == text;
}

// Keep physical byte offsets while treating continued comments/directives as
// one logical line. Both Unix and Windows source line endings are accepted.
std::size_t logical_line_end(std::string_view source, std::size_t start) {
    auto end = source.find('\n', start);
    while (end != std::string_view::npos) {
        auto before = end;
        if (before > start && source[before - 1] == '\r') --before;
        if (before == start || source[before - 1] != '\\') return end;
        end = source.find('\n', end + 1);
    }
    return source.size();
}

std::size_t quoted_end(std::string_view source, std::size_t start, char quote) {
    for (std::size_t i = start + 1; i < source.size(); ++i) {
        if (source[i] == '\\') {
            ++i;
        } else if (source[i] == quote) {
            return i + 1;
        }
    }
    throw Diagnostic("unterminated string or character literal", start);
}

std::size_t raw_end(std::string_view source, std::size_t start, std::size_t prefix_size) {
    // The delimiter is the text after R" and before (. Braces inside are opaque.
    const auto delim_start = start + prefix_size;
    const auto open = source.find('(', delim_start);
    if (open == std::string_view::npos || open - delim_start > 16) {
        throw Diagnostic("invalid raw string delimiter", start);
    }
    for (std::size_t i = delim_start; i < open; ++i) {
        if (source[i] == '\\' || source[i] == ')' || source[i] == ' ' ||
            source[i] == '\t' || source[i] == '\n' || source[i] == '\r') {
            throw Diagnostic("invalid raw string delimiter", start);
        }
    }
    std::string closing = ")";
    closing += source.substr(delim_start, open - delim_start);
    closing += '"';
    const auto close = source.find(closing, open + 1);
    if (close == std::string_view::npos) {
        throw Diagnostic("unterminated raw string literal", start);
    }
    return close + closing.size();
}

} // namespace

std::vector<Token> tokenize(std::string_view source) {
    std::vector<Token> tokens;
    std::size_t i = 0;
    bool line_start = true;

    while (i < source.size()) {
        const auto begin = i;
        const char ch = source[i];

        if (ch == '\n') {
            ++i;
            line_start = true;
            continue;
        }
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\f' || ch == '\v') {
            ++i;
            continue;
        }
        if (starts_with_at(source, i, "//")) {
            i = logical_line_end(source, i);
            continue;
        }
        if (starts_with_at(source, i, "/*")) {
            const auto end = source.find("*/", i + 2);
            if (end == std::string_view::npos) {
                throw Diagnostic("unterminated block comment", i);
            }
            if (source.substr(i, end + 2 - i).find('\n') != std::string_view::npos)
                line_start = true;
            i = end + 2;
            continue;
        }
        if (ch == '#' && line_start) {
            i = logical_line_end(source, i);
            tokens.push_back({Kind::Directive, begin, i});
            line_start = false;
            continue;
        }

        line_start = false;
        std::size_t raw_prefix = 0;
        for (const auto prefix : {"u8R\"", "uR\"", "UR\"", "LR\"", "R\""}) {
            if (starts_with_at(source, i, prefix)) {
                raw_prefix = std::string_view(prefix).size();
                break;
            }
        }
        if (raw_prefix != 0) {
            i = raw_end(source, i, raw_prefix);
            tokens.push_back({Kind::Literal, begin, i});
        } else if (ch == '"' || ch == '\'') {
            i = quoted_end(source, i, ch);
            tokens.push_back({Kind::Literal, begin, i});
        } else if (is_identifier_start(ch)) {
            ++i;
            while (i < source.size() && is_identifier_continue(source[i])) ++i;
            tokens.push_back({Kind::Identifier, begin, i});
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            ++i;
            while (i < source.size() &&
                   (is_identifier_continue(source[i]) || source[i] == '.' ||
                    (source[i] == '\'' && i + 1 < source.size() &&
                     is_identifier_continue(source[i + 1])) ||
                    ((source[i] == '+' || source[i] == '-') &&
                     (source[i - 1] == 'e' || source[i - 1] == 'E' ||
                      source[i - 1] == 'p' || source[i - 1] == 'P')))) ++i;
            tokens.push_back({Kind::Number, begin, i});
        } else {
            ++i;
            tokens.push_back({Kind::Punctuation, begin, i});
        }
    }
    return tokens;
}

} // namespace charmsharp::detail


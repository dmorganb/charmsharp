#pragma once
// Phase 1 standalone lexical classifier. Not used by the Charm# translator.
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace charm_metrics {
enum class Language { Cpp, CharmInterface, CharmSharp };
enum class Category { Operator, Operand };
struct Token { Category category; std::string spelling; std::size_t offset; };

inline std::vector<Token> lex(std::string_view s, Language lang) {
    static const std::unordered_set<std::string_view> cpp = {
        "alignas","alignof","asm","auto","bool","break","case","catch","char",
        "char8_t","char16_t","char32_t","class","co_await","co_return","co_yield",
        "concept","const","consteval","constexpr","constinit","const_cast",
        "continue","decltype","default","delete","do","double","dynamic_cast",
        "else","enum","explicit","export","extern","float","for",
        "friend","goto","if","inline","int","long","mutable","namespace","new",
        "noexcept","operator","private","protected","public","register",
        "reinterpret_cast","requires","return","short","signed","sizeof","static",
        "static_assert","static_cast","struct","switch","template","this",
        "thread_local","throw","try","typedef","typeid","typename",
        "union","unsigned","using","virtual","void","volatile","wchar_t","while",
        "and","and_eq","bitand","bitor","compl","not","not_eq","or","or_eq",
        "xor","xor_eq"
    };
    static const std::unordered_set<std::string_view> charm = {
        "module","mainmodule","chare","mainchare","group","nodegroup","array",
        "entry","message","readonly","initnode","initproc","initcall","pupable",
        "threaded","sync","iget","immediate","expedited","createhere","createhome",
        "nokeep","notrace","local","when","overlap","serial","atomic","connect",
        "forward","stacksize","reductiontarget"
    };
    const auto begins = [&](std::size_t pos, std::string_view value) {
        return s.substr(pos, value.size()) == value;
    };
    const auto ident = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    };
    std::vector<Token> tokens;
    auto emit = [&](Category category, std::size_t a, std::size_t b) {
        tokens.push_back({category, std::string(s.substr(a, b-a)), a});
    };
    bool line_start = true;
    for (std::size_t i = 0; i < s.size();) {
        char c = s[i];
        if (c == '\n') { ++i; line_start = true; continue; }
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
        if (begins(i, "//")) {
            auto end = s.find('\n', i + 2);
            i = end == s.npos ? s.size() : end; continue;
        }
        if (begins(i, "/*")) {
            auto end = s.find("*/", i + 2);
            if (end == s.npos) throw std::runtime_error("unterminated block comment");
            if (s.substr(i, end+2-i).find('\n') != s.npos) line_start = true;
            i = end + 2; continue;
        }
        if (line_start && c == '#') {
            for (;;) {
                auto end = s.find('\n', i);
                if (end == s.npos) { i = s.size(); break; }
                bool continued = end > i && s[end-1] == '\\';
                i = end + 1;
                if (!continued) break;
            }
            line_start = true; continue;
        }
        line_start = false;
        // Literal prefixes, including C++ raw string delimiters.
        std::size_t q = i;
        bool raw = false;
        for (auto p : {"u8R\"","uR\"","UR\"","LR\"","R\""}) {
            if (begins(i, p)) { q = i + std::string_view(p).size(); raw = true; break; }
        }
        if (raw) {
            auto open = s.find('(', q);
            if (open == s.npos || open-q > 16)
                throw std::runtime_error("invalid raw string delimiter");
            std::string close = ")" + std::string(s.substr(q, open-q)) + "\"";
            auto end = s.find(close, open+1);
            if (end == s.npos) throw std::runtime_error("unterminated raw string");
            emit(Category::Operand, i, end+close.size());
            i = end+close.size(); continue;
        }
        for (auto p : {"u8","u","U","L"}) {
            if (begins(i, p)) {
                auto at = i + std::string_view(p).size();
                if (at < s.size() && (s[at] == '\"' || s[at] == '\'')) { q = at; break; }
            }
        }
        if (s[q] == '\"' || s[q] == '\'') {
            auto quote = s[q++];
            bool found = false;
            while (q < s.size()) {
                if (s[q] == '\\') { q += 2; continue; }
                if (s[q++] == quote) { found = true; break; }
            }
            if (!found) throw std::runtime_error("unterminated quoted literal");
            while (q < s.size() && ident(s[q])) ++q; // user-defined suffix
            emit(Category::Operand, i, q); i = q; continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            auto end = i+1;
            while (end < s.size() && ident(s[end])) ++end;
            auto spelling = s.substr(i, end-i);
            bool op = cpp.contains(spelling) || (lang != Language::Cpp && charm.contains(spelling));
            emit(op ? Category::Operator : Category::Operand, i, end);
            i = end; continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i+1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i+1])))) {
            auto end = i+1;
            while (end < s.size()) {
                char n = s[end];
                if (ident(n) || n == '.' || n == '\'') { ++end; continue; }
                if ((n == '+' || n == '-') && end > i &&
                    (s[end-1] == 'e' || s[end-1] == 'E' ||
                     s[end-1] == 'p' || s[end-1] == 'P')) { ++end; continue; }
                break;
            }
            emit(Category::Operand, i, end); i = end; continue;
        }
        static constexpr std::string_view operators[] = {
            "<=>","->*","<<=",">>=","...","::","->",".*","++","--","<<",">>",
            "<=",">=","==","!=","&&","||","+=","-=","*=","/=","%=",
            "&=","|=","^=","[[","]]","##"
        };
        std::size_t end = i+1;
        for (auto op : operators)
            if (op.size() > end-i && begins(i, op)) end = i+op.size();
        emit(Category::Operator, i, end);
        i = end;
    }
    return tokens;
}
} // namespace charm_metrics

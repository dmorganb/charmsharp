#include "charmsharp/translator.hpp"
#include "lexer.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace charmsharp {

Diagnostic::Diagnostic(std::string message, std::size_t offset)
    : std::runtime_error(std::move(message)), offset_(offset) {}

std::string format_diagnostic(std::string_view source, const Diagnostic& error) {
    std::size_t line = 1;
    std::size_t column = 1;
    for (std::size_t i = 0; i < std::min(error.offset(), source.size()); ++i) {
        if (source[i] == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    return std::to_string(line) + ":" + std::to_string(column) + ": " + error.what();
}

namespace {

using detail::Kind;
using detail::Token;

struct Edit {
    std::size_t begin;
    std::size_t end;
    std::string replacement;
};

struct Chare {
    std::string kind;
    std::string name;
    std::vector<std::string> entries;
};

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    return text;
}

bool valid_identifier(std::string_view name) {
    if (name.empty() || !detail::is_identifier_start(name.front())) return false;
    return std::all_of(name.begin() + 1, name.end(), detail::is_identifier_continue);
}

class Parser {
public:
    Parser(std::string_view source, std::string_view module)
        : source_(source), module_(module), tokens_(detail::tokenize(source)) {}

    Generated run() {
        std::size_t depth = 0;
        int conditional_depth = 0;
        for (std::size_t i = 0; i < tokens_.size(); ++i) {
            if (tokens_[i].kind == Kind::Directive) {
                const auto directive = trim(source_.substr(tokens_[i].begin + 1,
                                                            tokens_[i].end - tokens_[i].begin - 1));
                const auto directive_name = directive.substr(0, directive.find_first_of(" \t\r("));
                if (directive_name == "ifdef" || directive_name == "ifndef" ||
                    directive_name == "if") {
                    ++conditional_depth;
                } else if (directive_name == "endif") {
                    --conditional_depth;
                }
                continue;
            }
            if (is(i, "{")) {
                ++depth;
            } else if (is(i, "}")) {
                if (depth != 0) --depth;
            } else if (is(i, "chare") || is(i, "mainchare")) {
                if (depth != 0) fail(i, "nested/namespace chares are not supported in phase 1");
                if (conditional_depth != 0)
                    fail(i, "a chare inside a preprocessor conditional is not supported");
                i = parse_chare(i) - 1;
            } else if (depth == 0 &&
                       (is(i, "group") || is(i, "nodegroup") ||
                        is(i, "message") || is(i, "readonly") ||
                        (is(i, "array") && is(i + 1, "[")))) {
                fail(i, "this Charm# declaration is reserved for a later milestone");
            }
        }
        if (chares_.empty()) throw Diagnostic("expected a top-level 'chare' or 'mainchare' declaration", 0);

        std::string cpp(source_);
        // Applying backwards keeps all source offsets stable.
        std::stable_sort(edits_.begin(), edits_.end(), [](const Edit& a, const Edit& b) {
            return a.begin > b.begin;
        });
        for (const auto& edit : edits_) {
            cpp.replace(edit.begin, edit.end - edit.begin, edit.replacement);
        }
        cpp.insert(0, "#include \"" + module_ + ".decl.h\"\n");
        if (cpp.empty() || cpp.back() != '\n') cpp += '\n';
        cpp += "#include \"" + module_ + ".def.h\"\n";

        std::string ci = "module " + module_ + " {\n";
        for (const auto& chare : chares_) {
            ci += "    " + chare.kind + " " + chare.name + " {\n";
            for (const auto& signature : chare.entries)
                ci += "        entry " + signature + ";\n";
            ci += "    };\n";
        }
        ci += "};\n";
        return {std::move(ci), std::move(cpp)};
    }

private:
    std::string_view source_;
    std::string module_;
    std::vector<Token> tokens_;
    std::vector<Edit> edits_;
    std::vector<Chare> chares_;

    bool is(std::size_t i, std::string_view word) const {
        return i < tokens_.size() &&
               source_.substr(tokens_[i].begin, tokens_[i].end - tokens_[i].begin) == word;
    }

    [[noreturn]] void fail(std::size_t i, std::string message) const {
        throw Diagnostic(std::move(message),
                         i < tokens_.size() ? tokens_[i].begin : source_.size());
    }

    std::size_t matching(std::size_t open, std::string_view left,
                         std::string_view right, std::size_t limit) const {
        int nesting = 0;
        for (std::size_t i = open; i < limit; ++i) {
            if (is(i, left)) ++nesting;
            else if (is(i, right) && --nesting == 0) return i;
        }
        fail(open, "unbalanced '" + std::string(left) + "' delimiter");
    }

    std::size_t parse_chare(std::size_t keyword) {
        const std::string kind(is(keyword, "mainchare") ? "mainchare" : "chare");
        const auto name = keyword + 1;
        if (name >= tokens_.size() || tokens_[name].kind != Kind::Identifier)
            fail(keyword, "expected a chare name");
        const std::string chare_name(source_.substr(tokens_[name].begin,
                                                    tokens_[name].end - tokens_[name].begin));
        if (std::any_of(chares_.begin(), chares_.end(), [&](const Chare& chare) {
            return chare.name == chare_name;
        })) fail(name, "duplicate chare name");
        if (kind == "mainchare" && std::any_of(chares_.begin(), chares_.end(), [](const Chare& chare) {
            return chare.kind == "mainchare";
        })) fail(keyword, "only one mainchare is allowed per module");
        const auto brace = name + 1;
        if (is(brace, ":")) fail(brace, "chare inheritance is deferred in phase 1");
        if (!is(brace, "{")) fail(brace, "expected '{' after chare name");
        const auto close = matching(brace, "{", "}", tokens_.size());
        if (!is(close + 1, ";")) fail(close, "expected ';' after chare body");

        edits_.push_back({tokens_[keyword].begin, tokens_[keyword].end, "class"});
        edits_.push_back({tokens_[name].end, tokens_[name].end,
                          " : public CBase_" + chare_name});
        chares_.push_back({kind, chare_name, {}});
        parse_members(brace + 1, close, chares_.back());
        return close + 2;
    }

    void parse_members(std::size_t begin, std::size_t end, Chare& chare) {
        int brace_depth = 0;
        bool in_entry = false;
        for (std::size_t i = begin; i < end;) {
            if (brace_depth == 0 && tokens_[i].kind == Kind::Directive)
                fail(i, "preprocessor directives between chare members are deferred");
            if (brace_depth == 0 && is(i + 1, ":") &&
                (is(i, "entry") || is(i, "public") || is(i, "private") ||
                 is(i, "protected"))) {
                in_entry = is(i, "entry");
                if (in_entry)
                    edits_.push_back({tokens_[i].begin, tokens_[i].end, "public"});
                i += 2;
                continue;
            }
            if (brace_depth == 0 && in_entry) {
                i = parse_entry(i, end, chare);
                continue;
            }
            if (is(i, "{")) ++brace_depth;
            else if (is(i, "}") && brace_depth != 0) --brace_depth;
            ++i;
        }
    }

    std::size_t parse_entry(std::size_t begin, std::size_t end, Chare& chare) {
        if (tokens_[begin].kind == Kind::Directive)
            fail(begin, "preprocessor directives between entry methods are deferred");
        if (is(begin, "[")) fail(begin, "entry attributes are deferred in phase 1");

        std::size_t open = begin;
        while (open < end && !is(open, "(")) {
            if (is(open, ";") || is(open, "{") || is(open, "="))
                fail(begin, "entry: permits only simple method declarations/definitions");
            ++open;
        }
        if (open == end || open == begin || tokens_[open - 1].kind != Kind::Identifier)
            fail(begin, "expected a simple entry method signature");

        const auto name = std::string(source_.substr(tokens_[open - 1].begin,
                                                      tokens_[open - 1].end - tokens_[open - 1].begin));
        if (name == chare.name) {
            if (open != begin + 1) fail(begin, "constructor entry must have no return type");
        } else if (!(open == begin + 2 && is(begin, "void"))) {
            fail(begin, "phase 1 supports only 'void name(...)' and constructors");
        }

        const auto close = matching(open, "(", ")", end);
        for (std::size_t i = open + 1; i < close; ++i) {
            if (is(i, "=")) fail(i, "default entry arguments are deferred in phase 1");
        }
        if (!is(close + 1, ";") && !is(close + 1, "{"))
            fail(close, "entry qualifiers, trailing return types and initializer lists are deferred");

        const auto signature = trim(source_.substr(tokens_[begin].begin,
                                                    tokens_[close].end - tokens_[begin].begin));
        if (std::find(chare.entries.begin(), chare.entries.end(), signature) != chare.entries.end())
            fail(begin, "duplicate entry method signature");
        chare.entries.emplace_back(signature);

        if (is(close + 1, ";")) return close + 2;
        const auto body_end = matching(close + 1, "{", "}", end);
        for (std::size_t i = close + 2; i < body_end; ++i) {
            if (is(i, "when") || is(i, "overlap") || is(i, "forall") ||
                (is(i, "case") && is(i + 1, "{"))) {
                fail(i, "SDAG syntax is deferred; phase 1 accepts ordinary C++ bodies only");
            }
        }
        return is(body_end + 1, ";") ? body_end + 2 : body_end + 1;
    }
};

} // namespace

Generated translate(std::string_view source, std::string_view module_name) {
    if (!valid_identifier(module_name))
        throw Diagnostic("module name must be a C++ identifier", 0);
    return Parser(source, module_name).run();
}

} // namespace charmsharp

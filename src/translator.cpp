#include "charmsharp/translator.hpp"
#include "lexer.hpp"
#include "sdag.hpp"

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
    struct Method {
        std::string name;
        std::string signature;
        std::size_t begin;
        std::size_t end;
        bool definition;
    };
    struct Receive {
        std::string name;
        std::string signature;
        std::size_t token;
    };
    std::string kind;
    std::string name;
    std::string parallel_base;
    std::vector<std::string> entries;
    std::vector<std::string> sdag_entries;
    std::vector<Method> methods;
    std::vector<Receive> receives;
    bool has_sdag = false;
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
        collect_type_names();
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
            } else if (is(i, "chare") || is(i, "mainchare") || is(i, "group") ||
                       is(i, "nodegroup") || (is(i, "array") && is(i + 1, "["))) {
                if (depth != 0) fail(i, "nested/namespace parallel types are not supported");
                if (conditional_depth != 0)
                    fail(i, "a parallel type inside a preprocessor conditional is not supported");
                i = parse_chare(i) - 1;
            } else if (is(i, "message")) {
                if (depth != 0) fail(i, "nested/namespace messages are not supported");
                if (conditional_depth != 0)
                    fail(i, "a message inside a preprocessor conditional is not supported");
                i = parse_message(i) - 1;
            } else if (is(i, "readonly")) {
                if (depth != 0) fail(i, "readonly declarations must be at global scope");
                if (conditional_depth != 0)
                    fail(i, "readonly inside a preprocessor conditional is not supported");
                i = parse_readonly(i) - 1;
            }
        }
        // Out-of-line definitions of marshalled entries must use pointer
        // signatures too; otherwise a runtime-sized array is not valid C++20.
        for (std::size_t i = 0; i + 4 < tokens_.size(); ++i) {
            if (!is(i + 1, ":") || !is(i + 2, ":") || !is(i + 4, "(")) continue;
            const auto class_name = token_text(i);
            const auto method_name = token_text(i + 3);
            if (std::find(array_entry_methods_.begin(), array_entry_methods_.end(),
                          std::make_pair(class_name, method_name)) == array_entry_methods_.end())
                continue;
            const auto close = matching(i + 4, "(", ")", tokens_.size());
            rewrite_entry_arrays(i + 4, close);
            i = close;
        }
        if (chares_.empty() && messages_.empty() && readonly_.empty())
            throw Diagnostic("expected a top-level Charm# declaration", 0);

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

        const bool has_mainchare = std::any_of(chares_.begin(), chares_.end(), [](const Chare& chare) {
            return chare.kind == "mainchare";
        });
        std::string ci = std::string(has_mainchare ? "mainmodule " : "module ") + module_ + " {\n";
        for (const auto& message : messages_)
            ci += "    message " + message + ";\n";
        for (const auto& declaration : readonly_)
            ci += "    readonly " + declaration + ";\n";
        for (const auto& chare : chares_) {
            ci += "    " + chare.kind + " " + chare.name;
            if (!chare.parallel_base.empty()) ci += " : " + chare.parallel_base;
            ci += " {\n";
            for (const auto& signature : chare.entries)
                ci += "        entry " + signature + ";\n";
            for (const auto& definition : chare.sdag_entries)
                ci += "        entry " + definition + ";\n";
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
    std::vector<std::pair<std::string, std::string>> array_entry_methods_;
    std::vector<std::string> messages_;
    std::vector<std::string> readonly_;
    std::vector<std::string> readonly_names_;
    std::vector<std::string> parallel_type_names_;
    std::vector<std::string> message_type_names_;
    std::vector<std::pair<std::string, std::size_t>> ordinary_type_names_;

    bool is(std::size_t i, std::string_view word) const {
        return i < tokens_.size() &&
               source_.substr(tokens_[i].begin, tokens_[i].end - tokens_[i].begin) == word;
    }

    std::string token_text(std::size_t i) const {
        return std::string(source_.substr(tokens_[i].begin,
                                          tokens_[i].end - tokens_[i].begin));
    }

    void collect_type_names() {
        // Resolve locally declared parallel bases regardless of source order;
        // do not mistake these for ordinary C++ classes during lowering.
        std::size_t depth = 0;
        for (std::size_t i = 0; i < tokens_.size(); ++i) {
            if (is(i, "{")) { ++depth; continue; }
            if (is(i, "}")) { if (depth) --depth; continue; }
            if (depth != 0 || tokens_[i].kind == Kind::Directive) continue;
            if ((is(i, "class") || is(i, "struct")) && i + 2 < tokens_.size() &&
                tokens_[i + 1].kind == Kind::Identifier && is(i + 2, "{")) {
                ordinary_type_names_.emplace_back(token_text(i + 1), tokens_[i].begin);
            }
            if (is(i, "message") && i + 1 < tokens_.size() &&
                tokens_[i + 1].kind == Kind::Identifier)
                message_type_names_.push_back(token_text(i + 1));
            std::size_t name = i + 1;
            if (is(i, "array") && is(i + 1, "[")) {
                const auto bracket = matching(i + 1, "[", "]", tokens_.size());
                if (is(bracket + 1, "chare")) name = bracket + 2;
            } else if (!is(i, "chare") && !is(i, "mainchare") &&
                       !is(i, "group") && !is(i, "nodegroup")) {
                continue;
            }
            if (name < tokens_.size() && tokens_[name].kind == Kind::Identifier)
                parallel_type_names_.push_back(token_text(name));
        }
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

    bool name_used(std::string_view name) const {
        return std::find(messages_.begin(), messages_.end(), name) != messages_.end() ||
               std::find(readonly_names_.begin(), readonly_names_.end(), name) != readonly_names_.end() ||
               std::any_of(chares_.begin(), chares_.end(), [&](const Chare& chare) {
                   return chare.name == name;
               });
    }

    struct Bases {
        std::size_t brace;
        std::string cpp;
        std::string parallel;
    };

    Bases parse_ordinary_bases(std::size_t after_name, std::size_t declaration,
                               bool is_message) const {
        Bases result{after_name, "", ""};
        if (!is(after_name, ":")) return result;
        auto i = after_name + 1;
        std::vector<std::string> seen;
        while (i < tokens_.size() && !is(i, "{")) {
            const auto begin = i;
            if (is(i, "virtual")) fail(i, "virtual base inheritance is deferred");
            const bool explicit_public = is(i, "public");
            if (is(i, "public") || is(i, "private") || is(i, "protected")) ++i;
            if (i >= tokens_.size() || tokens_[i].kind != Kind::Identifier)
                fail(i, "expected a simple base class name");
            const auto base = token_text(i);
            if (is(i, "virtual")) fail(i, "virtual base inheritance is deferred");
            if (std::find(seen.begin(), seen.end(), base) != seen.end())
                fail(i, "duplicate base class");
            seen.push_back(base);
            if (is_message && std::find(message_type_names_.begin(), message_type_names_.end(), base) !=
                                  message_type_names_.end())
                fail(i, "message-to-message inheritance is deferred");
            if (std::find(parallel_type_names_.begin(), parallel_type_names_.end(), base) !=
                parallel_type_names_.end()) {
                if (is_message) fail(i, "message inheritance from parallel types is not supported");
                if (!explicit_public)
                    fail(i, "parallel bases require explicit public inheritance");
                if (!result.parallel.empty())
                    fail(i, "multiple parallel bases are deferred");
                const auto existing = std::find_if(chares_.begin(), chares_.end(),
                                                   [&](const Chare& chare) { return chare.name == base; });
                if (existing == chares_.end())
                    fail(i, "parallel base must be defined before derived type");
                if (existing->kind != "chare")
                    fail(i, "only chare-to-chare inheritance is currently supported");
                result.parallel = base;
                ++i;
                if (is(i, ",")) {
                    ++i;
                    if (is(i, "{")) fail(i, "expected a base class after ','");
                    continue;
                }
                if (!is(i, "{")) fail(i, "complex base specifiers are deferred");
                continue;
            }
            if (std::none_of(ordinary_type_names_.begin(), ordinary_type_names_.end(),
                             [&](const auto& type) {
                                 return type.first == base && type.second < tokens_[declaration].begin;
                             }))
                fail(i, "unresolved or later-defined C++ base type");
            ++i;
            if (!result.cpp.empty()) result.cpp += ", ";
            result.cpp += std::string(trim(source_.substr(
                tokens_[begin].begin, tokens_[i - 1].end - tokens_[begin].begin)));
            if (is(i, ",")) {
                ++i;
                if (is(i, "{")) fail(i, "expected a base class after ','");
                continue;
            }
            if (!is(i, "{")) fail(i, "complex base specifiers are deferred");
        }
        result.brace = i;
        if (result.cpp.empty() && result.parallel.empty())
            fail(after_name, "expected a base class");
        return result;
    }

    std::size_t parse_readonly(std::size_t keyword) {
        // Charmxi accepts a single-token type, optionally followed by a
        // compile-time sized array. In this first slice, reject pointers,
        // references, initializers, and multi-token C++ type expressions.
        const auto type = keyword + 1;
        const auto name = type + 1;
        if (type >= tokens_.size() || tokens_[type].kind != Kind::Identifier ||
            name >= tokens_.size() || tokens_[name].kind != Kind::Identifier)
            fail(keyword, "expected readonly Type name; (single-token type)");
        const std::string variable(source_.substr(tokens_[name].begin,
                                                  tokens_[name].end - tokens_[name].begin));
        if (name_used(variable)) fail(name, "duplicate Charm# type or readonly name");
        auto end = name + 1;
        if (is(end, "[")) {
            const auto bound = end + 1;
            if (bound >= tokens_.size() || tokens_[bound].kind != Kind::Number ||
                !is(bound + 1, "]"))
                fail(end, "readonly arrays require a fixed numeric bound");
            const auto value = source_.substr(tokens_[bound].begin,
                                              tokens_[bound].end - tokens_[bound].begin);
            if (value.empty() || value[0] == '0' ||
                !std::all_of(value.begin(), value.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
                fail(bound, "readonly array bound must be a positive decimal integer");
            end = bound + 2;
        }
        if (!is(end, ";"))
            fail(end, "readonly initializers, pointers and complex declarators are deferred");
        readonly_.emplace_back(trim(source_.substr(tokens_[type].begin,
                                                   tokens_[end].begin - tokens_[type].begin)));
        readonly_names_.push_back(variable);
        edits_.push_back({tokens_[keyword].begin, tokens_[type].begin, ""});
        return end + 1;
    }

    std::size_t parse_message(std::size_t keyword) {
        const auto name = keyword + 1;
        if (name >= tokens_.size() || tokens_[name].kind != Kind::Identifier)
            fail(keyword, "expected a message type name");
        const std::string message_name(source_.substr(tokens_[name].begin,
                                                      tokens_[name].end - tokens_[name].begin));
        if (name_used(message_name)) fail(name, "duplicate Charm# type name");
        const auto bases = parse_ordinary_bases(name + 1, keyword, true);
        const auto brace = bases.brace;
        if (!is(brace, "{")) fail(brace, "expected '{' after message type name");
        const auto close = matching(brace, "{", "}", tokens_.size());
        if (!is(close + 1, ";")) fail(close, "expected ';' after message body");

        // A plain .ci declaration generates the CMessage_ base; fixed-size
        // fields and ordinary C++ methods remain verbatim in the class body.
        // Reject unsized array fields: those require the special variable-size
        // .ci metadata and allocation semantics not yet implemented here.
        for (auto i = brace + 1; i < close; ++i) {
            if (is(i, "[") && is(i + 1, "]"))
                fail(i, "variable-size message arrays are deferred");
            if (is(i, "entry") && is(i + 1, ":"))
                fail(i, "entry: is only valid in parallel types");
        }
        edits_.push_back({tokens_[keyword].begin, tokens_[name].begin, "class "});
        edits_.push_back({tokens_[name].end, tokens_[brace].begin,
                          " : public CMessage_" + message_name +
                          (bases.cpp.empty() ? " " : ", " + bases.cpp + " ")});
        messages_.push_back(message_name);
        return close + 2;
    }

    std::size_t parse_chare(std::size_t keyword) {
        std::string kind;
        auto name = keyword + 1;
        if (is(keyword, "array")) {
            // The lexer retains e.g. '1D' as a single numeric token. Spaces in
            // '1 D' are also allowed; normalize both forms for the .ci output.
            if (!is(name, "[")) fail(name, "expected '[' after array");
            const auto dim_begin = name + 1;
            const auto dim_end = matching(name, "[", "]", tokens_.size());
            std::string dimension;
            for (auto i = dim_begin; i < dim_end; ++i)
                dimension += source_.substr(tokens_[i].begin, tokens_[i].end - tokens_[i].begin);
            if (dimension.size() < 2 || dimension.back() != 'D' ||
                !std::all_of(dimension.begin(), dimension.end() - 1, [](char c) {
                    return c >= '0' && c <= '9';
                }) || dimension[0] == '0')
                fail(dim_begin, "array dimension must be a positive integer followed by D");
            if (!is(dim_end + 1, "chare")) fail(dim_end + 1, "expected 'chare' after array dimension");
            kind = "array [" + dimension + "]";
            name = dim_end + 2;
        } else {
            kind = std::string(source_.substr(tokens_[keyword].begin,
                                             tokens_[keyword].end - tokens_[keyword].begin));
        }
        if (name >= tokens_.size() || tokens_[name].kind != Kind::Identifier)
            fail(keyword, "expected a parallel type name");
        const std::string chare_name(source_.substr(tokens_[name].begin,
                                                    tokens_[name].end - tokens_[name].begin));
        if (name_used(chare_name)) fail(name, "duplicate chare name or Charm# type name");
        if (kind == "mainchare" && std::any_of(chares_.begin(), chares_.end(), [](const Chare& chare) {
            return chare.kind == "mainchare";
        })) fail(keyword, "only one mainchare is allowed per module");
        const auto bases = parse_ordinary_bases(name + 1, keyword, false);
        const auto brace = bases.brace;
        if (!is(brace, "{")) fail(brace, "expected '{' after parallel type name");
        const auto close = matching(brace, "{", "}", tokens_.size());
        if (!is(close + 1, ";")) fail(close, "expected ';' after chare body");

        edits_.push_back({tokens_[keyword].begin, tokens_[name].begin, "class "});
        edits_.push_back({tokens_[name].end, tokens_[brace].begin,
                          " : public CBase_" + chare_name +
                          (bases.cpp.empty() ? " " : ", " + bases.cpp + " ")});
        chares_.push_back({kind, chare_name, bases.parallel, {}, {}, {}, {}, false});
        parse_members(brace + 1, close, chares_.back());
        auto& chare = chares_.back();
        for (const auto& receive : chare.receives) {
            auto existing = std::find_if(chare.methods.begin(), chare.methods.end(),
                                         [&](const Chare::Method& method) {
                                             return method.name == receive.name;
                                         });
            if (existing == chare.methods.end()) {
                chare.entries.push_back(receive.signature);
            } else {
                if (existing->signature != receive.signature)
                    fail(receive.token, "incompatible explicit SDAG receive entry declaration");
                if (existing->definition)
                    fail(receive.token, "SDAG receive entry cannot have a C++ implementation");
                // The SDAG macro supplies the C++ receive method.
                edits_.push_back({existing->begin, existing->end, ""});
            }
        }
        if (chare.has_sdag) {
            edits_.push_back({tokens_[close].begin, tokens_[close].begin,
                              "\npublic:\n    " + chare.name + "_SDAG_CODE\n"});
        }
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


    bool rewrite_entry_arrays(std::size_t open, std::size_t close) {
        bool found = false;
        std::vector<std::string> scalar_lengths;
        auto first = open + 1;
        for (auto i = first; i <= close; ++i) {
            // Only a simple comma-separated parameter list is interpreted for
            // marshalled arrays; other C++ signatures remain untouched.
            if (i != close && !is(i, ",")) continue;
            if (i == first) { first = i + 1; continue; }
            std::size_t bracket = first;
            while (bracket < i && !is(bracket, "[")) ++bracket;
            if (bracket != i) {
                if (bracket != first + 2 || bracket + 3 != i ||
                    tokens_[first].kind != Kind::Identifier ||
                    tokens_[first + 1].kind != Kind::Identifier ||
                    !is(bracket + 2, "]"))
                    fail(first, "unsupported marshalled array parameter declarator");
                const auto type = token_text(first);
                if (type != "float" && type != "double" && type != "int")
                    fail(first, "unsupported marshalled array element type");
                const auto bound = token_text(bracket + 1);
                const auto numeric = !bound.empty() &&
                    std::all_of(bound.begin(), bound.end(),
                                [](char c) { return c >= '0' && c <= '9'; });
                if (numeric) {
                    if (bound[0] == '0')
                        fail(bracket + 1, "marshalled array literal bound must be positive");
                } else if (std::find(scalar_lengths.begin(), scalar_lengths.end(), bound) ==
                           scalar_lengths.end()) {
                    fail(bracket + 1, "marshalled array bound must name an earlier integer parameter");
                }
                // In .ci keep T data[n]; C++ receives T *data.
                edits_.push_back({tokens_[first + 1].begin, tokens_[first + 1].begin, "*"});
                edits_.push_back({tokens_[bracket].begin, tokens_[bracket + 2].end, ""});
                found = true;
            } else if (i == first + 2 && is(first, "int") &&
                       tokens_[first + 1].kind == Kind::Identifier) {
                scalar_lengths.push_back(token_text(first + 1));
            }
            first = i + 1;
        }
        return found;
    }

    std::size_t parse_entry(std::size_t begin, std::size_t end, Chare& chare) {
        if (tokens_[begin].kind == Kind::Directive)
            fail(begin, "preprocessor directives between entry methods are deferred");

        std::vector<std::string> attributes;
        const auto entry_start = begin;
        while (is(begin, "[") && is(begin + 1, "[")) {
            // [[charm::threaded]], [[charm::sync]] and [[charm::reductiontarget]] map to .ci
            // attributes; remove them from the generated ordinary C++ class.
            if (begin + 7 >= end || !is(begin + 2, "charm") ||
                !is(begin + 3, ":") || !is(begin + 4, ":") ||
                tokens_[begin + 5].kind != Kind::Identifier ||
                !is(begin + 6, "]") || !is(begin + 7, "]"))
                fail(begin, "expected [[charm::threaded]], [[charm::sync]] or [[charm::reductiontarget]]");
            const auto attribute = std::string(source_.substr(tokens_[begin + 5].begin,
                                                              tokens_[begin + 5].end - tokens_[begin + 5].begin));
            if (attribute != "threaded" && attribute != "sync" && attribute != "reductiontarget")
                fail(begin + 5, "unsupported Charm++ entry attribute");
            if (std::find(attributes.begin(), attributes.end(), attribute) != attributes.end())
                fail(begin, "duplicate Charm++ entry attribute");
            attributes.push_back(attribute);
            edits_.push_back({tokens_[begin].begin, tokens_[begin + 7].end, ""});
            begin += 8;
        }
        if (is(begin, "[")) fail(begin, "unsupported entry attribute syntax");
        if (begin == end) fail(entry_start, "expected entry method after attribute");

        const bool threaded = std::find(attributes.begin(), attributes.end(), "threaded") != attributes.end();
        const bool sync = std::find(attributes.begin(), attributes.end(), "sync") != attributes.end();
        if (threaded && sync) fail(entry_start, "threaded and sync cannot be combined");
        const bool reductiontarget = std::find(attributes.begin(), attributes.end(), "reductiontarget") != attributes.end();
        if (reductiontarget && (threaded || sync))
            fail(entry_start, "reductiontarget cannot be combined with threaded or sync");

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
            if (!attributes.empty()) fail(entry_start, "entry constructors cannot have Charm++ entry attributes");
        } else if (!(open == begin + 2 && is(begin, "void"))) {
            // Charm++ synchronous entries can return a pointer to a message.
            if (!(sync && open == begin + 3 &&
                  tokens_[begin].kind == Kind::Identifier && is(begin + 1, "*")))
                fail(begin, "entry methods must return void, or a message pointer with [[charm::sync]]");
        } else if (sync) {
            fail(begin, "sync entries must return a message pointer");
        }

        const auto close = matching(open, "(", ")", end);
        if (rewrite_entry_arrays(open, close))
            array_entry_methods_.emplace_back(chare.name, name);
        for (std::size_t i = open + 1; i < close; ++i) {
            if (is(i, "=")) fail(i, "default entry arguments are deferred in phase 1");
        }
        // Entry constructor initializer lists belong only in the C++ definition,
        // not in the .ci signature. Validate the structural shape while leaving
        // every source byte (including comments/spacing) untouched.
        auto body = close + 1;
        if (is(body, ":")) {
            if (name != chare.name)
                fail(body, "initializer lists are only valid for entry constructors");
            ++body;
            while (true) {
                // The member/base designator is intentionally restricted to a
                // simple or qualified identifier; full C++ parsing is deferred.
                if (body >= end || tokens_[body].kind != Kind::Identifier)
                    fail(body, "expected initializer member or base name");
                ++body;
                while (is(body, ":") && is(body + 1, ":")) {
                    body += 2;
                    if (body >= end || tokens_[body].kind != Kind::Identifier)
                        fail(body, "expected qualified initializer name");
                    ++body;
                }
                if (!is(body, "(") && !is(body, "{"))
                    fail(body, "expected '(' or '{' after initializer name");
                const bool paren = is(body, "(");
                body = matching(body, paren ? "(" : "{", paren ? ")" : "}", end) + 1;
                if (!is(body, ",")) break;
                ++body;
            }
            if (!is(body, "{"))
                fail(body, "expected constructor body after initializer list");
        }
        if (!is(body, ";") && !is(body, "{"))
            fail(body, "entry qualifiers, trailing return types and initializer lists are deferred");

        std::string signature;
        for (const auto& attribute : attributes) signature += "[" + attribute + "] ";
        signature += trim(source_.substr(tokens_[begin].begin,
                                                    tokens_[close].end - tokens_[begin].begin));
        if (std::find(chare.entries.begin(), chare.entries.end(), signature) != chare.entries.end())
            fail(begin, "duplicate entry method signature");

        if (is(body, ";")) {
            chare.entries.emplace_back(signature);
            chare.methods.push_back({name, signature, tokens_[begin].begin, tokens_[body].end, false});
            return body + 1;
        }
        const auto body_end = matching(body, "{", "}", end);
        bool candidate_sdag = false;
        std::size_t first_sdag = body + 1;
        for (std::size_t i = body + 1; i < body_end; ++i) {
            if (is(i, "when") || is(i, "overlap") || is(i, "forall") ||
                (is(i, "case") && is(i + 1, "{"))) {
                candidate_sdag = true;
                first_sdag = i;
                break;
            }
        }
        if (candidate_sdag) {
            const auto first_byte = tokens_[body].begin;
            const auto text = source_.substr(first_byte, tokens_[body_end].end - first_byte);
            detail::SdagBody analyzed;
            try {
                analyzed = detail::parse_sdag_body(text);
            } catch (const detail::SdagError& error) {
                throw Diagnostic(error.what(), first_byte + error.offset());
            }
            if (!analyzed.has_sdag || analyzed.statements.size() != 1 ||
                analyzed.statements[0].kind != detail::SdagKind::When ||
                name == chare.name || !attributes.empty())
                fail(first_sdag, "Phase 5.1 supports one unannotated void entry with one when");
            auto receive = body + 1;
            if (!is(receive, "when"))
                fail(first_sdag, "Phase 5.1 requires when as the only entry statement");
            ++receive;
            if (receive >= body_end || tokens_[receive].kind != Kind::Identifier)
                fail(receive, "expected SDAG receive entry name");
            const auto receive_name = token_text(receive++);
            if (!is(receive, "("))
                fail(receive, "Phase 5.1 does not support reference-matched when");
            const auto receive_close = matching(receive, "(", ")", body_end);
            if (is(receive_close + 1, ","))
                fail(receive_close + 1, "Phase 5.1 does not support multiple receives");
            const auto receive_signature = "void " + receive_name + std::string(
                source_.substr(tokens_[receive].begin,
                               tokens_[receive_close].end - tokens_[receive].begin));
            const auto serial_or_brace = receive_close + 1;
            const bool explicit_serial = is(serial_or_brace, "serial");
            const auto receive_brace = explicit_serial ? serial_or_brace + 1 : serial_or_brace;
            if (!is(receive_brace, "{"))
                fail(receive_brace, "expected when receive body");
            const auto receive_end = matching(receive_brace, "{", "}", body_end);
            if (receive_end + 1 != body_end)
                fail(receive_end + 1, "Phase 5.1 requires a single when statement");
            // This slice only accepts ordinary C++ inside one serial block.
            for (auto i = receive_brace + 1; i < receive_end; ++i) {
                if (is(i, "when") || is(i, "overlap") || is(i, "forall") ||
                    (is(i, "case") && is(i + 1, "{")))
                    fail(i, "nested SDAG awaits are deferred");
            }
            std::string ci_body(text);
            if (!explicit_serial) {
                const auto offset = tokens_[receive_brace].begin - first_byte;
                ci_body.insert(offset, "serial ");
            }
            chare.sdag_entries.push_back(signature + " " + ci_body);
            chare.receives.push_back({receive_name, receive_signature, receive - 1});
            chare.has_sdag = true;
            edits_.push_back({tokens_[begin].begin, tokens_[body_end].end, ""});
        } else {
            chare.entries.emplace_back(signature);
            chare.methods.push_back({name, signature, tokens_[begin].begin,
                                     tokens_[body_end].end, true});
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

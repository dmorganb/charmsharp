#include "charmsharp/translator.hpp"
#include "lexer.hpp"
#include "sdag.hpp"
#include "semantic.hpp"
#include "ci_emitter.hpp"
#include "cpp_emitter.hpp"

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

struct ParsedTranslation {
    Generated generated;
    detail::SemanticModel semantic;
};

struct Chare {
    struct Receive {
        std::string name;
        std::vector<std::string> parameter_key;
        std::size_t token;
        detail::SourceRange parameters;
    };
    std::string kind;
    std::string name;
    std::vector<Receive> receives;
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
        : source_(source), module_(module), tokens_(detail::tokenize(source)) {
        semantic_.module_name = module_;
    }

    ParsedTranslation run(bool emit_output = true) {
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
            } else if (is(i, "template") && is(i + 1, "chare")) {
                if (depth == 0 && conditional_depth == 0)
                    i = parse_template_registration(i) - 1;
            } else if (is(i, "template") && is(i + 1, "<")) {
                const auto close = matching(i + 1, "<", ">", tokens_.size());
                if (is(close + 1, "chare")) {
                    if (depth != 0) fail(i, "nested/namespace parallel types are not supported");
                    if (conditional_depth != 0)
                        fail(i, "a parallel type inside a preprocessor conditional is not supported");
                    i = parse_chare(close + 1, i, close) - 1;
                } else if (is(close + 1, "group") || is(close + 1, "nodegroup") ||
                           is(close + 1, "mainchare") || is(close + 1, "array")) {
                    fail(i, "only scalar chare templates are supported in Phase 6.3.1");
                }
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
            analyze_entry_arrays(i + 4, close);
            i = close;
        }
        discover_template_registrations();
        if (chares_.empty() && messages_.empty() && readonly_.empty())
            throw Diagnostic("expected a top-level Charm# declaration", 0);

        semantic_.is_mainmodule = std::any_of(semantic_.parallel_types.begin(),
            semantic_.parallel_types.end(), [](const detail::ParallelDeclaration& chare) {
                return chare.kind == detail::ParallelKind::Mainchare;
            });
        if (!emit_output) return {{}, std::move(semantic_)};

        auto cpp = detail::emit_cpp(source_, semantic_);
        auto ci = detail::emit_ci(source_, semantic_);
        auto registration_ci = detail::emit_registration_ci(semantic_);
        auto template_header = detail::emit_template_header(source_, semantic_);
        return {{std::move(ci), std::move(cpp), std::move(registration_ci),
                 std::move(template_header)}, std::move(semantic_)};
    }

private:
    std::string_view source_;
    std::string module_;
    std::vector<Token> tokens_;
    detail::SemanticModel semantic_;
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
        std::optional<std::string> array_bound;
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
            array_bound = std::string(value);
            end = bound + 2;
        }
        if (!is(end, ";"))
            fail(end, "readonly initializers, pointers and complex declarators are deferred");
        readonly_.emplace_back(trim(source_.substr(tokens_[type].begin,
                                                   tokens_[end].begin - tokens_[type].begin)));
        readonly_names_.push_back(variable);
        detail::ReadonlyDeclaration declaration;
        declaration.type_name = token_text(type);
        declaration.name = variable;
        declaration.array_bound = std::move(array_bound);
        declaration.declarator = {tokens_[type].begin, tokens_[end].begin};
        declaration.source = {tokens_[keyword].begin, tokens_[end].end};
        semantic_.readonly_variables.push_back(std::move(declaration));
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
            // Method bodies, parameter lists and initializer expressions are
            // ordinary C++. In particular, [] there may be a lambda capture.
            if (is(i, "{") || is(i, "(")) {
                const bool paren = is(i, "(");
                i = matching(i, paren ? "(" : "{", paren ? ")" : "}", close);
                continue;
            }
            if (is(i, "[") && is(i + 1, "]") && i > brace + 1 &&
                tokens_[i - 1].kind == Kind::Identifier && !is(i - 1, "operator"))
                fail(i, "variable-size message arrays are deferred");
            if (is(i, "entry") && is(i + 1, ":"))
                fail(i, "entry: is only valid in parallel types");
        }
        messages_.push_back(message_name);
        semantic_.messages.push_back({message_name, bases.cpp,
                                      {tokens_[keyword].begin, tokens_[close + 1].end},
                                      {tokens_[name].begin, tokens_[name].end},
                                      tokens_[brace].begin});
        return close + 2;
    }

    std::size_t parse_template_registration(std::size_t start) {
        // A Charm# registration declaration is intentionally distinct from
        // the C++ explicit class-instantiation syntax.
        const auto name_token = start + 2;
        if (name_token >= tokens_.size() || tokens_[name_token].kind != Kind::Identifier)
            return start + 1;
        const auto name = token_text(name_token);
        const auto declared = std::find_if(semantic_.parallel_types.begin(),
            semantic_.parallel_types.end(), [&](const detail::ParallelDeclaration& declaration) {
                return declaration.name == name && !declaration.template_declaration.empty();
            });
        if (declared == semantic_.parallel_types.end())
            fail(name_token, "template chare registration requires a preceding template chare declaration");
        const auto open = name_token + 1;
        if (!is(open, "<"))
            fail(open, "expected template specialization arguments in explicit registration");
        const auto close = matching(open, "<", ">", tokens_.size());
        if (!is(close + 1, ";"))
            fail(close + 1, "expected ';' after explicit chare template registration");
        if (close == open + 1)
            fail(open, "template registration requires a concrete type argument");
        const auto argument = std::string(trim(source_.substr(tokens_[open + 1].begin,
            tokens_[close - 1].end - tokens_[open + 1].begin)));
        const auto type_param = declared->template_arguments.substr(
            1, declared->template_arguments.size() - 2);
        if (argument == type_param)
            fail(open, "template registration requires a concrete specialization");
        const auto args = std::string(source_.substr(
            tokens_[open].begin, tokens_[close].end - tokens_[open].begin));
        if (std::any_of(semantic_.template_registrations.begin(),
                semantic_.template_registrations.end(), [&](const auto& previous) {
                    return previous.name == name && registration_key(previous.arguments) == registration_key(args);
                }))
            fail(start, "duplicate explicit chare template specialization");
        semantic_.template_registrations.push_back({
            name, args, {tokens_[start].begin, tokens_[close + 1].end}});
        return close + 2;
    }

    // Registration identity ignores formatting; Charmxi only needs one
    // registration for each concrete specialization.
    static std::string registration_key(std::string_view arguments) {
        std::string key;
        for (const auto& token : detail::tokenize(arguments)) {
            if (token.kind == Kind::Directive) continue;
            const auto part = arguments.substr(token.begin, token.end - token.begin);
            key += std::to_string(part.size()) + ":" + std::string(part);
        }
        return key;
    }

    void discover_template_registrations() {
        // This deliberately is not C++ template argument deduction. Only a
        // known scalar chare proxy with one concrete built-in type qualifies.
        // The tokenizer excludes comments, strings, and raw-string contents.
        const std::vector<std::string> builtin_types = {
            "bool", "char", "signed char", "unsigned char", "short",
            "short int", "unsigned short", "unsigned short int",
            "int", "signed", "signed int", "unsigned", "unsigned int",
            "long", "long int", "unsigned long", "unsigned long int",
            "long long", "long long int", "unsigned long long",
            "unsigned long long int", "float", "double", "long double"
        };
        for (std::size_t i = 0; i + 2 < tokens_.size(); ++i) {
            if (tokens_[i].kind != Kind::Identifier || !is(i + 1, "<")) continue;
            const auto proxy = token_text(i);
            constexpr std::string_view prefix = "CProxy_";
            if (proxy.compare(0, prefix.size(), prefix) != 0) continue;
            const auto name = proxy.substr(prefix.size());
            const auto declaration = std::find_if(semantic_.parallel_types.begin(),
                semantic_.parallel_types.end(), [&](const auto& entry) {
                    return entry.name == name && entry.kind == detail::ParallelKind::Chare &&
                           !entry.template_declaration.empty();
                });
            if (declaration == semantic_.parallel_types.end()) continue;
            const auto close = matching(i + 1, "<", ">", tokens_.size());
            std::string type;
            bool simple = close > i + 2;
            for (auto j = i + 2; j < close; ++j) {
                if (tokens_[j].kind != Kind::Identifier) { simple = false; break; }
                if (!type.empty()) type += " ";
                type += token_text(j);
            }
            if (!simple || std::find(builtin_types.begin(), builtin_types.end(), type) ==
                               builtin_types.end()) continue;
            const auto args = "<" + type + ">";
            const auto key = registration_key(args);
            const bool exists = std::any_of(semantic_.template_registrations.begin(),
                semantic_.template_registrations.end(), [&](const auto& existing) {
                    return existing.name == name && registration_key(existing.arguments) == key;
                });
            if (!exists)
                semantic_.template_registrations.push_back({name, args, {}});
            i = close;
        }
    }

    std::size_t parse_chare(std::size_t keyword, std::size_t template_keyword = 0,
                            std::size_t template_close = 0) {
        std::string template_declaration;
        std::string template_arguments;
        if (template_close != 0) {
            // Restrict the initial slice to one ordinary type parameter. The
            // full template-argument grammar belongs to a later phase.
            const auto type_keyword = template_keyword + 2;
            const auto parameter = template_keyword + 3;
            if ((!is(type_keyword, "typename") && !is(type_keyword, "class")) ||
                parameter + 1 != template_close ||
                tokens_[parameter].kind != Kind::Identifier)
                fail(template_keyword, "chare templates currently require one typename/class parameter");
            template_declaration = std::string(source_.substr(tokens_[template_keyword].begin,
                tokens_[template_close].end - tokens_[template_keyword].begin));
            template_arguments = "<" + token_text(parameter) + ">";
        }
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
        // Only a scalar `chare Main` is an implicit mainchare. Array/group
        // declarations named Main retain their normal parallel kinds.
        if (kind == "chare" && chare_name == "Main") kind = "mainchare";
        const auto previous_main = std::find_if(chares_.begin(), chares_.end(),
            [](const Chare& chare) { return chare.kind == "mainchare"; });
        if (kind == "mainchare" && previous_main != chares_.end())
            fail(keyword, "only one mainchare is allowed per module (already identified " +
                 previous_main->name + ")");
        const auto bases = parse_ordinary_bases(name + 1, keyword, false);
        const auto brace = bases.brace;
        if (!is(brace, "{")) fail(brace, "expected '{' after parallel type name");
        const auto close = matching(brace, "{", "}", tokens_.size());
        if (!is(close + 1, ";")) fail(close, "expected ';' after chare body");

        chares_.push_back({kind, chare_name, {}});
        detail::ParallelDeclaration semantic_chare;
        semantic_chare.kind = is(keyword, "array") ? detail::ParallelKind::Array :
                              kind == "mainchare" ? detail::ParallelKind::Mainchare :
                              is(keyword, "group") ? detail::ParallelKind::Group :
                              is(keyword, "nodegroup") ? detail::ParallelKind::Nodegroup :
                              detail::ParallelKind::Chare;
        semantic_chare.name = chare_name;
        semantic_chare.template_declaration = template_declaration;
        semantic_chare.template_arguments = template_arguments;
        if (semantic_chare.kind == detail::ParallelKind::Array)
            semantic_chare.array_dimension = kind.substr(7, kind.size() - 9);
        semantic_chare.parallel_base = bases.parallel;
        semantic_chare.ordinary_bases = bases.cpp;
        semantic_chare.source = {tokens_[keyword].begin, tokens_[close + 1].end};
        semantic_chare.name_source = {tokens_[name].begin, tokens_[name].end};
        semantic_chare.opening_brace = tokens_[brace].begin;
        semantic_chare.closing_brace = tokens_[close].begin;
        semantic_.parallel_types.push_back(std::move(semantic_chare));
        parse_members(brace + 1, close, chares_.back());
        auto& chare = chares_.back();
        auto& entries = semantic_.parallel_types.back().entries;
        std::vector<Chare::Receive> registered_receives;
        for (const auto& receive : chare.receives) {
            const auto repeated = std::find_if(registered_receives.begin(), registered_receives.end(),
                [&](const Chare::Receive& registered) { return registered.name == receive.name; });
            if (repeated != registered_receives.end()) {
                if (repeated->parameter_key != receive.parameter_key)
                    fail(receive.token, "inconsistent SDAG receive signatures");
                continue;
            }
            registered_receives.push_back(receive);
            auto existing = std::find_if(entries.begin(), entries.end(),
                [&](const detail::EntryDeclaration& entry) {
                    return entry.name == receive.name && entry.parameter_key == receive.parameter_key;
                });
            if (existing == entries.end()) {
                if (std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
                        return entry.name == receive.name;
                    }))
                    fail(receive.token, "incompatible explicit SDAG receive entry declaration");
                detail::EntryDeclaration inferred;
                inferred.name = receive.name;
                inferred.return_type = "void";
                inferred.parameter_list = std::string(source_.substr(
                    receive.parameters.begin, receive.parameters.end - receive.parameters.begin));
                inferred.parameter_key = receive.parameter_key;
                inferred.signature = receive.parameters;
                inferred.source = receive.parameters;
                inferred.is_receive = true;
                inferred.is_implicit = true;
                entries.push_back(std::move(inferred));
            } else {
                if (existing->return_type.empty() || !existing->attributes.empty())
                    fail(receive.token, "incompatible explicit SDAG receive entry declaration");
                if (existing->is_definition)
                    fail(receive.token, "SDAG receive entry cannot have a C++ implementation");
                existing->is_receive = true;
            }
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
                    semantic_.parallel_types.back().entry_labels.push_back(
                        {tokens_[i].begin, tokens_[i].end});
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


    // Normalize supported simple declarators without reconstructing emitted text.
    // Complex C++ declarators retain their tokens; type aliases are not resolved.
    std::vector<std::string> parameter_key(std::size_t open, std::size_t close) const {
        std::vector<std::string> key;
        std::vector<std::pair<std::string, std::string>> names;
        auto first = open + 1;
        int depth = 0;
        std::size_t parameter_index = 0;
        for (auto i = first; i <= close; ++i) {
            if (i != close) {
                if (is(i, "(") || is(i, "[") || is(i, "<")) ++depth;
                if (is(i, ")") || is(i, "]") || is(i, ">")) --depth;
                if (!is(i, ",") || depth != 0) continue;
            }
            auto declarator_end = first;
            while (declarator_end < i && !is(declarator_end, "[")) ++declarator_end;
            auto name = i; // No name identified in an opaque/unnamed parameter.
            const auto is_type_word = [&](std::size_t token) {
                const auto word = token_text(token);
                return word == "void" || word == "bool" || word == "char" ||
                       word == "int" || word == "float" || word == "double" ||
                       word == "short" || word == "long" || word == "signed" ||
                       word == "unsigned" || word == "const" || word == "volatile" ||
                       word == "auto" || word == "wchar_t" || word == "char8_t" ||
                       word == "char16_t" || word == "char32_t";
            };
            if (declarator_end > first + 1 &&
                tokens_[declarator_end - 1].kind == Kind::Identifier &&
                !is_type_word(declarator_end - 1) && !is(declarator_end - 2, ":")) {
                bool simple = true;
                bool has_type = false;
                for (auto t = first; t + 1 < declarator_end; ++t) {
                    if (is(t, "struct") || is(t, "class") || is(t, "enum") || is(t, "typename"))
                        simple = false;
                    if (tokens_[t].kind != Kind::Identifier && !is(t, ":") &&
                        !is(t, "*") && !is(t, "&")) simple = false;
                    if (tokens_[t].kind == Kind::Identifier &&
                        !is(t, "const") && !is(t, "volatile")) has_type = true;
                }
                if (simple && has_type) name = declarator_end - 1;
            }
            if (name != i)
                names.emplace_back(token_text(name), "$" + std::to_string(parameter_index));
            for (auto t = first; t < i; ++t) {
                if (t == name) continue;
                auto word = token_text(t);
                if (t > declarator_end) {
                    const auto bound = std::find_if(names.begin(), names.end(),
                        [&](const auto& parameter) { return parameter.first == word; });
                    if (bound != names.end()) word = bound->second;
                }
                key.push_back(std::move(word));
            }
            if (i != close) key.emplace_back(",");
            ++parameter_index;
            first = i + 1;
        }
        if (key.size() == 1 && key.front() == "void") key.clear();
        return key;
    }

    bool analyze_entry_arrays(std::size_t open, std::size_t close) {
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
                // Preserve the original declarator for both independent backends.
                semantic_.marshalled_arrays.push_back({
                    {tokens_[first + 1].begin, tokens_[first + 1].end},
                    {tokens_[bracket].begin, tokens_[bracket + 2].end}
                });
                found = true;
            } else if (i == first + 2 && is(first, "int") &&
                       tokens_[first + 1].kind == Kind::Identifier) {
                scalar_lengths.push_back(token_text(first + 1));
            }
            first = i + 1;
        }
        return found;
    }

    // Analyze SDAG before emission: validate suspension-sensitive constructs
    // and register receives. The .ci emitter only renders validated nodes.
    void analyze_sdag_nodes(std::string_view text, std::size_t first_byte,
                            const std::vector<detail::SdagNode>& nodes, Chare& chare,
                            detail::EntryDeclaration& entry) {
        for (std::size_t n = 0; n < nodes.size();) {
            const auto& node = nodes[n];
            if (node.kind == detail::SdagKind::Ordinary) {
                const bool followed_by_wait = std::any_of(
                    nodes.begin() + static_cast<std::ptrdiff_t>(n + 1),
                    nodes.end(), [](const detail::SdagNode& statement) {
                        return statement.kind == detail::SdagKind::When ||
                               statement.kind == detail::SdagKind::For ||
                               statement.kind == detail::SdagKind::If ||
                               statement.kind == detail::SdagKind::While ||
                               statement.kind == detail::SdagKind::Forall ||
                               statement.kind == detail::SdagKind::Overlap ||
                               statement.kind == detail::SdagKind::Case;
                    });
                if (followed_by_wait) {
                    auto token = std::size_t{0};
                    while (token < tokens_.size() &&
                           tokens_[token].begin < first_byte + node.begin)
                        ++token;
                    const auto first = token_text(token);
                    if (first == "int" || first == "float" || first == "double" ||
                        first == "bool" || first == "char" || first == "auto" ||
                        first == "const" || first == "unsigned" ||
                        first == "signed" || first == "short" || first == "long")
                        fail(token, "local declaration before an SDAG wait requires a chare field");
                }
                // Emission groups ordinary runs into serial blocks, but every
                // statement needs validation, not just the first in each run.
                ++n;
                continue;
            }
            analyze_sdag_node(text, first_byte, node, chare, entry);
            ++n;
        }
    }

    void analyze_sdag_node(std::string_view text, std::size_t first_byte,
                           const detail::SdagNode& node, Chare& chare,
                           detail::EntryDeclaration& entry) {
        if (node.kind == detail::SdagKind::Ordinary ||
            node.kind == detail::SdagKind::Serial)
            return;
        if (node.kind == detail::SdagKind::Block) {
            analyze_sdag_nodes(text, first_byte, node.children, chare, entry);
            return;
        }
        if (node.kind == detail::SdagKind::If || node.kind == detail::SdagKind::For ||
            node.kind == detail::SdagKind::While || node.kind == detail::SdagKind::Forall ||
            node.kind == detail::SdagKind::Overlap || node.kind == detail::SdagKind::Case) {
            if (node.kind == detail::SdagKind::For) {
                auto token = std::size_t{0};
                while (token < tokens_.size() &&
                       tokens_[token].begin < first_byte + node.begin)
                    ++token;
                if (is(token, "for") && is(token + 1, "(")) {
                    const auto type = token_text(token + 2);
                    if (type == "int" || type == "auto" || type == "long" ||
                        type == "unsigned" || type == "short" || type == "std")
                        fail(token + 2, "SDAG for index must be a persistent chare field");
                }
            }
            if (node.kind == detail::SdagKind::Forall) {
                auto token = std::size_t{0};
                while (token < tokens_.size() &&
                       tokens_[token].begin < first_byte + node.begin)
                    ++token;
                if (is(token, "forall") && is(token + 1, "[") &&
                    (tokens_[token + 2].kind != Kind::Identifier ||
                     !is(token + 3, "]")))
                    fail(token + 2, "SDAG forall index must name a persistent chare field");
            }
            for (const auto& child : node.children)
                analyze_sdag_node(text, first_byte, child, chare, entry);
            return;
        }
        if (node.kind != detail::SdagKind::When)
            throw Diagnostic("unsupported SDAG control statement",
                             first_byte + node.begin);

        auto receive = std::size_t{0};
        while (receive < tokens_.size() &&
               tokens_[receive].begin < first_byte + node.begin)
            ++receive;
        if (!is(receive, "when"))
            fail(receive, "expected a direct SDAG when statement");
        ++receive;
        if (receive >= tokens_.size() || tokens_[receive].kind != Kind::Identifier)
            fail(receive, "expected SDAG receive entry name");
        const auto receive_name = token_text(receive++);
        if (is(receive, "[")) {
            // Reference matching is evaluated by Charmxi; the first entry
            // argument carries the reference number for marshalled entries.
            receive = matching(receive, "[", "]", tokens_.size()) + 1;
        }
        if (!is(receive, "("))
            fail(receive, "expected SDAG receive parameters");
        const auto receive_close = matching(receive, "(", ")", tokens_.size());
        if (is(receive_close + 1, ","))
            fail(receive_close + 1, "multiple receives in one when statement are not supported");
        const auto serial_or_brace = receive_close + 1;
        const bool explicit_serial = is(serial_or_brace, "serial");
        const auto receive_brace = explicit_serial ? serial_or_brace + 1 : serial_or_brace;
        if (!is(receive_brace, "{"))
            fail(receive_brace, "expected when receive body");
        const auto receive_end = matching(receive_brace, "{", "}", tokens_.size());
        for (auto i = receive_brace + 1; i < receive_end; ++i) {
            if (is(i, "when") || is(i, "overlap") || is(i, "forall") ||
                (is(i, "case") && is(i + 1, "{")))
                fail(i, "nested SDAG awaits are deferred");
        }
        chare.receives.push_back({receive_name, parameter_key(receive, receive_close), receive - 1,
                                  {tokens_[receive].begin, tokens_[receive_close].end}});
        entry.sdag_receives.push_back({
            {first_byte + node.begin, first_byte + node.end},
            tokens_[receive_brace].begin,
            explicit_serial
        });
    }

    std::size_t parse_entry(std::size_t begin, std::size_t end, Chare& chare) {
        if (tokens_[begin].kind == Kind::Directive)
            fail(begin, "preprocessor directives between entry methods are deferred");

        std::vector<std::string> attributes;
        std::vector<detail::SourceRange> attribute_ranges;
        const auto entry_start = begin;
        while (is(begin, "[") && is(begin + 1, "[")) {
            // Supported [[charm::attribute]] markers map to .ci
            // attributes; remove them from the generated ordinary C++ class.
            if (begin + 7 >= end || !is(begin + 2, "charm") ||
                !is(begin + 3, ":") || !is(begin + 4, ":") ||
                tokens_[begin + 5].kind != Kind::Identifier ||
                !is(begin + 6, "]") || !is(begin + 7, "]"))
                fail(begin, "expected [[charm::attribute]]");
            const auto attribute = std::string(source_.substr(tokens_[begin + 5].begin,
                                                              tokens_[begin + 5].end - tokens_[begin + 5].begin));
            if (attribute != "threaded" && attribute != "sync" &&
                attribute != "reductiontarget" && attribute != "notrace" &&
                attribute != "appwork" && attribute != "nokeep" &&
                attribute != "exclusive" &&
                attribute != "inline" && attribute != "local")
                fail(begin + 5, "unsupported Charm++ entry attribute");
            if (std::find(attributes.begin(), attributes.end(), attribute) != attributes.end())
                fail(begin, "duplicate Charm++ entry attribute");
            attributes.push_back(attribute);
            attribute_ranges.push_back({tokens_[begin].begin, tokens_[begin + 7].end});
            begin += 8;
        }
        if (is(begin, "[")) fail(begin, "unsupported entry attribute syntax");
        if (begin == end) fail(entry_start, "expected entry method after attribute");

        const bool threaded = std::find(attributes.begin(), attributes.end(), "threaded") != attributes.end();
        const bool sync = std::find(attributes.begin(), attributes.end(), "sync") != attributes.end();
        if (threaded && sync) fail(entry_start, "threaded and sync cannot be combined");
        const bool local = std::find(attributes.begin(), attributes.end(), "local") != attributes.end();
        if (local && chare.kind != "group" && chare.kind.rfind("array [", 0) != 0)
            fail(entry_start, "local entry attribute requires a group or chare array");
        if (std::find(attributes.begin(), attributes.end(), "exclusive") != attributes.end() &&
            chare.kind != "nodegroup")
            fail(entry_start, "exclusive entry attribute requires a nodegroup");
        const bool reductiontarget = std::find(attributes.begin(), attributes.end(), "reductiontarget") != attributes.end();
        if (reductiontarget && (threaded || sync))
            fail(entry_start, "reductiontarget cannot be combined with threaded or sync");
        // Phase 6.2a permits the two independent instrumentation markers
        // together; combinations with scheduling/ownership attributes require
        // separate Charmxi-backed compatibility tests.
        if (attributes.size() > 1 &&
            !(attributes.size() == 2 &&
              std::find(attributes.begin(), attributes.end(), "notrace") != attributes.end() &&
              std::find(attributes.begin(), attributes.end(), "appwork") != attributes.end()))
            fail(entry_start, "unsupported Charm++ entry attribute combination");

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
            if (!((sync && open == begin + 3 &&
                   tokens_[begin].kind == Kind::Identifier && is(begin + 1, "*")) ||
                  (local && (open == begin + 2 ||
                   (open == begin + 3 && is(begin + 1, "*"))))))
                fail(begin, "entry methods must return void, a sync message pointer, or a simple local return type");
        } else if (sync) {
            fail(begin, "sync entries must return a message pointer");
        }

        const auto close = matching(open, "(", ")", end);
        if (analyze_entry_arrays(open, close))
            array_entry_methods_.emplace_back(chare.name, name);
        for (std::size_t i = open + 1; i < close; ++i) {
            if (is(i, "=")) fail(i, "default entry arguments are not supported");
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
            fail(body, "entry qualifiers and trailing return types are not supported");

        const auto key = parameter_key(open, close);
        const auto& entries = semantic_.parallel_types.back().entries;
        if (std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
                return entry.name == name && entry.parameter_key == key;
            }))
            fail(begin, "duplicate entry method signature");

        detail::EntryDeclaration semantic_entry;
        semantic_entry.name = name;
        semantic_entry.parameter_key = key;
        if (name != chare.name)
            semantic_entry.return_type = std::string(trim(source_.substr(
                tokens_[begin].begin, tokens_[open - 1].begin - tokens_[begin].begin)));
        semantic_entry.parameter_list = std::string(source_.substr(
            tokens_[open].begin, tokens_[close].end - tokens_[open].begin));
        semantic_entry.attributes = attributes;
        semantic_entry.attribute_ranges = std::move(attribute_ranges);
        semantic_entry.signature = {tokens_[entry_start].begin, tokens_[close].end};
        semantic_entry.method_signature = {tokens_[begin].begin, tokens_[close].end};
        semantic_entry.is_definition = !is(body, ";");

        if (is(body, ";")) {
            semantic_entry.source = {tokens_[entry_start].begin, tokens_[body].end};
            semantic_.parallel_types.back().entries.push_back(std::move(semantic_entry));
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
            if (!analyzed.has_sdag || name == chare.name || !attributes.empty())
                fail(first_sdag, "SDAG requires an unannotated void entry method");
            analyze_sdag_nodes(text, first_byte, analyzed.statements,
                               chare, semantic_entry);
            semantic_entry.is_sdag = true;
            semantic_entry.sdag_body = std::move(analyzed);
            semantic_entry.sdag_body_start = first_byte;
        }
        semantic_entry.source = {tokens_[entry_start].begin, tokens_[body_end].end};
        semantic_.parallel_types.back().entries.push_back(std::move(semantic_entry));
        return is(body_end + 1, ";") ? body_end + 2 : body_end + 1;
    }
};

} // namespace

Generated translate(std::string_view source, std::string_view module_name) {
    if (!valid_identifier(module_name))
        throw Diagnostic("module name must be a C++ identifier", 0);
    return Parser(source, module_name).run().generated;
}

namespace detail {
SemanticModel analyze(std::string_view source, std::string_view module_name) {
    if (!valid_identifier(module_name))
        throw Diagnostic("module name must be a C++ identifier", 0);
    return Parser(source, module_name).run(false).semantic;
}
} // namespace detail

} // namespace charmsharp


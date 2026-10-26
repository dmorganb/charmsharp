#pragma once

#include "sdag.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace charmsharp::detail {

// Half-open byte ranges in the original .csh input. Opaque C++ text is
// deliberately represented by source ranges, not a partial C++ AST.
struct SourceRange {
    std::size_t begin = 0;
    std::size_t end = 0;
};

enum class ParallelKind { Chare, Mainchare, Array, Group, Nodegroup };

struct EntryDeclaration {
    std::string name;
    std::string return_type;      // Original spelling; empty for constructors.
    std::string parameter_list;   // Original spelling, including parentheses.
    std::vector<std::string> attributes; // Normalized Charm++ attribute names.
    SourceRange signature;
    SourceRange source;
    bool is_definition = false;
    bool is_sdag = false;
    bool is_receive = false;
    bool is_implicit = false;
    // Node offsets are relative to the method body beginning at sdag_body_start.
    std::optional<SdagBody> sdag_body;
    std::size_t sdag_body_start = 0;
};

struct ParallelDeclaration {
    ParallelKind kind = ParallelKind::Chare;
    std::string name;
    std::string array_dimension;  // Number without the D suffix; empty otherwise.
    std::string parallel_base;
    std::string ordinary_bases;   // Preserved C++ base-specifier text.
    SourceRange source;
    std::vector<EntryDeclaration> entries;
};

struct MessageDeclaration {
    std::string name;
    std::string ordinary_bases;
    SourceRange source;
};

struct ReadonlyDeclaration {
    std::string type_name;
    std::string name;
    std::optional<std::string> array_bound;
    SourceRange source;
};

struct SemanticModel {
    std::string module_name;
    bool is_mainmodule = false;
    std::vector<ParallelDeclaration> parallel_types;
    std::vector<MessageDeclaration> messages;
    std::vector<ReadonlyDeclaration> readonly_variables;
};

// PR1 exposes the model for direct tests and future backends. The current
// translator still emits .ci and C++ through its legacy path; PR2 and PR3
// will separate those emitters without changing this representation.
[[nodiscard]] SemanticModel analyze(std::string_view source,
                                    std::string_view module_name);

} // namespace charmsharp::detail

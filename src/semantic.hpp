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

// Source-level metadata for a validated marshalled array parameter.
// The C++ backend replaces the array suffix with a pointer declarator;
// the CI backend leaves the original source spelling intact.
struct MarshalledArray {
    SourceRange name;
    SourceRange suffix;
};

// Location of a when statement and its receive-body brace. The emitter only
// needs this layout to insert implicit serial, not to reparse the signature.
struct SdagReceiveLayout {
    SourceRange statement;
    std::size_t body_brace_begin = 0;
    bool explicit_serial = false;
};

struct EntryDeclaration {
    std::string name;
    std::string return_type;      // Original spelling; empty for constructors.
    std::string parameter_list;   // Original spelling, including parentheses.
    // Token identity for matching; simple parameter names are excluded.
    std::vector<std::string> parameter_key;
    std::vector<std::string> attributes; // Normalized Charm++ attribute names.
    std::vector<SourceRange> attribute_ranges; // Original [[charm::...]] spans.
    SourceRange signature;
    SourceRange method_signature; // Excludes Charm# attributes.
    SourceRange source;
    bool is_definition = false;
    bool is_sdag = false;
    bool is_receive = false;
    bool is_implicit = false;
    // Node offsets are relative to the method body beginning at sdag_body_start.
    std::optional<SdagBody> sdag_body;
    std::size_t sdag_body_start = 0;
    std::vector<SdagReceiveLayout> sdag_receives;
};

struct ParallelDeclaration {
    ParallelKind kind = ParallelKind::Chare;
    std::string name;
    std::string template_declaration; // Original `template<typename T>` prefix, if any.
    std::string template_arguments;   // E.g. `<T>` for generated CBase_Worker<T>.
    std::string array_dimension;  // Number without the D suffix; empty otherwise.
    std::string parallel_base;
    std::string ordinary_bases;   // Preserved C++ base-specifier text.
    SourceRange source;
    SourceRange name_source;
    std::size_t opening_brace = 0;
    std::size_t closing_brace = 0;
    std::vector<SourceRange> entry_labels;
    std::vector<EntryDeclaration> entries;
};

struct MessageDeclaration {
    std::string name;
    std::string ordinary_bases;
    SourceRange source;
    SourceRange name_source;
    std::size_t opening_brace = 0;
};

struct ReadonlyDeclaration {
    std::string type_name;
    std::string name;
    std::optional<std::string> array_bound;
    SourceRange declarator; // Original type, name and optional bound; no ';'.
    SourceRange source;
};

struct SemanticModel {
    std::string module_name;
    bool is_mainmodule = false;
    std::vector<ParallelDeclaration> parallel_types;
    std::vector<MessageDeclaration> messages;
    std::vector<ReadonlyDeclaration> readonly_variables;
    std::vector<MarshalledArray> marshalled_arrays;
};

// Internal analysis entry point for model tests and future backends. The
// C++ and .ci backends independently consume this model and source spans.
[[nodiscard]] SemanticModel analyze(std::string_view source,
                                    std::string_view module_name);

} // namespace charmsharp::detail


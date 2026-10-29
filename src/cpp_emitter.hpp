#pragma once

#include "semantic.hpp"
#include <string>
#include <string_view>

namespace charmsharp::detail {

// Apply C++-specific lowering from the validated Charm# semantic model.
// No Charm# parsing or semantic validation occurs in this backend.
[[nodiscard]] std::string emit_template_header(std::string_view source, const SemanticModel& model);
[[nodiscard]] std::string emit_cpp(std::string_view source, const SemanticModel& model);

// Shared-module backend: directly lowers validated source ranges, without
// reparsing the emitted C++ or stripping generated registration footers.
[[nodiscard]] std::string emit_shared_classes(std::string_view source, const SemanticModel& model);
[[nodiscard]] std::string emit_multi_cpp(std::string_view source, const SemanticModel& model);

} // namespace charmsharp::detail

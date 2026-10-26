#pragma once

#include "semantic.hpp"
#include <string>
#include <string_view>

namespace charmsharp::detail {

// Apply C++-specific lowering from the validated Charm# semantic model.
// No Charm# parsing or semantic validation occurs in this backend.
[[nodiscard]] std::string emit_cpp(std::string_view source, const SemanticModel& model);

} // namespace charmsharp::detail

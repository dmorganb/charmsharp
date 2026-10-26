#pragma once

#include "semantic.hpp"

#include <string>
#include <string_view>

namespace charmsharp::detail {

// Render conventional Charm++ interface text from a validated Charm# model.
// No Charm# lexing, receive registration, or semantic checks happen here.
[[nodiscard]] std::string emit_ci(std::string_view source,
                                  const SemanticModel& model);

} // namespace charmsharp::detail

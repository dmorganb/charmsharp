#pragma once

#include "semantic.hpp"

#include <string>
#include <string_view>

namespace charmsharp::detail {

// Render conventional Charm++ interface text from a validated Charm# model.
// No Charm# lexing, receive registration, or semantic checks happen here.
[[nodiscard]] std::string emit_ci(std::string_view source,
                                  const SemanticModel& model);
[[nodiscard]] std::string emit_registration_ci(const SemanticModel& model);
[[nodiscard]] std::string emit_ci_body(std::string_view source,
                                       const SemanticModel& model,
                                       bool include_specialization_extern = true);
[[nodiscard]] std::string emit_registration_ci_body(const SemanticModel& model);

} // namespace charmsharp::detail

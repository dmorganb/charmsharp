#pragma once

#include "semantic.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <cstddef>

namespace charmsharp::detail {

struct CiLineOrigin {
    std::size_t generated_line;
    std::size_t source_offset;
};

// Render conventional Charm++ interface text from a validated Charm# model.
// No Charm# lexing, receive registration, or semantic checks happen here.
[[nodiscard]] std::string emit_ci(std::string_view source,
                                  const SemanticModel& model);
[[nodiscard]] std::string emit_registration_ci(const SemanticModel& model);
[[nodiscard]] std::string emit_ci_body(std::string_view source,
                                       const SemanticModel& model,
                                       bool include_specialization_extern = true,
                                       std::vector<CiLineOrigin>* origins = nullptr);
[[nodiscard]] std::string emit_registration_ci_body(const SemanticModel& model);

} // namespace charmsharp::detail

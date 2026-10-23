#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace charmsharp {

struct Generated {
    std::string interface_source;
    std::string cpp_source;
};

class Diagnostic : public std::runtime_error {
public:
    Diagnostic(std::string message, std::size_t offset);
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

private:
    std::size_t offset_;
};

// Phase 1: a single top-level `chare`, simple entry methods, no SDAG.
[[nodiscard]] Generated translate(std::string_view source,
                                  std::string_view module_name);
[[nodiscard]] std::string format_diagnostic(std::string_view source,
                                            const Diagnostic& error);

} // namespace charmsharp

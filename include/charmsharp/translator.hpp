#pragma once

#include <cstddef>
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

// Translate the supported Charm# declaration and SDAG subset to Charm++
// interface and C++ sources. Invalid input reports a source-offset Diagnostic.
[[nodiscard]] Generated translate(std::string_view source,
                                  std::string_view module_name);
[[nodiscard]] std::string format_diagnostic(std::string_view source,
                                            const Diagnostic& error);

} // namespace charmsharp


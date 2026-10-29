#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace charmsharp {

struct Generated {
    std::string interface_source;
    std::string cpp_source;
    std::string registration_interface_source; // Optional separate Charm++ module.
    std::string template_header_source; // Optional shared class-template definitions.
};

// Multi-input interface aggregation. Each returned C++ translation unit retains
// its source-file identity. The caller owns output naming and build orchestration.
struct MultiGenerated {
    std::string interface_source;
    std::vector<std::string> cpp_sources;
};

[[nodiscard]] MultiGenerated translate_many(
    const std::vector<std::string_view>& sources, std::string_view module_name);

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


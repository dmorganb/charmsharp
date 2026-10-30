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

struct CiSourceLocation {
    std::size_t generated_line;
    std::string source_path;
    std::size_t source_line;
};

// Multi-input interface aggregation. Each returned C++ translation unit retains
// its source-file identity. The caller owns output naming and build orchestration.
struct MultiGenerated {
    std::string interface_source;
    std::vector<std::string> cpp_sources;
    std::string shared_header_source;
    std::string registration_cpp_source;
    std::string specialization_interface_source;
    std::vector<CiSourceLocation> interface_locations;
    std::vector<CiSourceLocation> specialization_locations;
};

[[nodiscard]] MultiGenerated translate_many(
    const std::vector<std::string_view>& sources, std::string_view module_name,
    const std::vector<std::string_view>& source_paths = {});

class Diagnostic : public std::runtime_error {
public:
    Diagnostic(std::string message, std::size_t offset);
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

private:
    std::size_t offset_;
};

class MultiDiagnostic : public Diagnostic {
public:
    MultiDiagnostic(std::string message, std::size_t file, std::size_t offset,
                    std::size_t previous_file = static_cast<std::size_t>(-1),
                    std::size_t previous_offset = 0);
    std::size_t file() const noexcept { return file_; }
    bool has_previous() const noexcept { return previous_file_ != static_cast<std::size_t>(-1); }
    std::size_t previous_file() const noexcept { return previous_file_; }
    std::size_t previous_offset() const noexcept { return previous_offset_; }
private:
    std::size_t file_;
    std::size_t previous_file_;
    std::size_t previous_offset_;
};

[[nodiscard]] std::string format_source_excerpt(std::string_view source, std::size_t offset);

// Translate the supported Charm# declaration and SDAG subset to Charm++
// interface and C++ sources. Invalid input reports a source-offset Diagnostic.
[[nodiscard]] Generated translate(std::string_view source,
                                  std::string_view module_name);
[[nodiscard]] std::string format_diagnostic(std::string_view source,
                                            const Diagnostic& error);

} // namespace charmsharp


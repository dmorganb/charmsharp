#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace charmsharp::detail {

enum class SdagKind {
    Ordinary,
    Block,
    Serial,
    When,
    If,
    For,
    While,
    Forall,
    Overlap,
    Case
};

struct SdagNode {
    SdagKind kind;
    std::size_t begin;
    std::size_t end;
    std::vector<SdagNode> children;
};

struct SdagBody {
    std::vector<SdagNode> statements;
    bool has_sdag = false;
};

class SdagError : public std::runtime_error {
public:
    SdagError(std::string message, std::size_t offset);
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
private:
    std::size_t offset_;
};

// Recognize a balanced method body, including its outer braces. All node
// source ranges are half-open byte offsets relative to the passed-in view.
// This structural representation is not yet an SDAG lowering or C++ AST.
[[nodiscard]] SdagBody parse_sdag_body(std::string_view body);

} // namespace charmsharp::detail

#include "sdag.hpp"
#include "lexer.hpp"

#include <string_view>
#include <utility>
#include <vector>

namespace charmsharp::detail {

SdagError::SdagError(std::string message, std::size_t offset)
    : std::runtime_error(std::move(message)), offset_(offset) {}

namespace {
class SdagParser {
public:
    explicit SdagParser(std::string_view text)
        : text_(text), tokens_(tokenize(text)) {}

    SdagBody parse() {
        if (!at(0, "{")) fail(0, "expected SDAG method body");
        std::size_t cursor = 0;
        auto body = block(cursor);
        if (cursor != tokens_.size()) fail(cursor, "unexpected tokens after SDAG body");
        SdagBody result;
        result.statements = std::move(body.children);
        result.has_sdag = contains_sdag(result.statements);
        return result;
    }

private:
    std::string_view text_;
    std::vector<Token> tokens_;

    bool at(std::size_t i, std::string_view word) const {
        return i < tokens_.size() &&
            text_.substr(tokens_[i].begin, tokens_[i].end - tokens_[i].begin) == word;
    }

    [[noreturn]] void fail(std::size_t i, const char* message) const {
        throw SdagError(message, i < tokens_.size() ? tokens_[i].begin : text_.size());
    }

    std::size_t pair(std::size_t start, std::string_view left,
                     std::string_view right) const {
        if (!at(start, left)) fail(start, "expected opening delimiter");
        std::size_t depth = 0;
        for (auto i = start; i < tokens_.size(); ++i) {
            if (at(i, left)) ++depth;
            if (at(i, right) && --depth == 0) return i;
        }
        fail(start, "unbalanced SDAG delimiter");
    }

    SdagNode block(std::size_t& cursor) {
        const auto start = cursor;
        if (!at(cursor, "{")) fail(cursor, "expected '{'");
        ++cursor;
        SdagNode node{SdagKind::Block, tokens_[start].begin, 0, {}};
        while (cursor < tokens_.size() && !at(cursor, "}"))
            node.children.push_back(statement(cursor));
        if (!at(cursor, "}")) fail(cursor, "unterminated SDAG block");
        node.end = tokens_[cursor++].end;
        return node;
    }

    SdagNode statement(std::size_t& cursor) {
        if (at(cursor, "{")) return block(cursor);
        if (at(cursor, "serial") && at(cursor + 1, "{")) {
            const auto start = cursor++;
            const auto close = pair(cursor, "{", "}");
            cursor = close + 1;
            return {SdagKind::Serial, tokens_[start].begin, tokens_[close].end, {}};
        }
        if (at(cursor, "when")) {
            const auto start = cursor++;
            do {
                if (cursor >= tokens_.size() || tokens_[cursor].kind != Kind::Identifier)
                    fail(cursor, "expected SDAG receive entry name");
                ++cursor;
                if (at(cursor, "[")) cursor = pair(cursor, "[", "]") + 1;
                if (!at(cursor, "(")) fail(cursor, "expected SDAG receive parameters");
                cursor = pair(cursor, "(", ")") + 1;
                if (!at(cursor, ",")) break;
                ++cursor;
            } while (true);
            if (!at(cursor, "{") && !(at(cursor, "serial") && at(cursor + 1, "{")))
                fail(cursor, "expected SDAG receive body");
            auto child = statement(cursor);
            return {SdagKind::When, tokens_[start].begin, child.end, {std::move(child)}};
        }
        if (at(cursor, "overlap") || (at(cursor, "case") && at(cursor + 1, "{"))) {
            const auto kind = at(cursor, "overlap") ? SdagKind::Overlap : SdagKind::Case;
            const auto start = cursor++;
            auto child = block(cursor);
            return {kind, tokens_[start].begin, child.end, {std::move(child)}};
        }
        if (at(cursor, "if") || at(cursor, "for") || at(cursor, "while") ||
            at(cursor, "forall")) {
            const auto start = cursor++;
            const auto kind = at(start, "if") ? SdagKind::If :
                              at(start, "for") ? SdagKind::For :
                              at(start, "while") ? SdagKind::While : SdagKind::Forall;
            if (kind == SdagKind::Forall) {
                if (!at(cursor, "[")) fail(cursor, "expected forall index");
                cursor = pair(cursor, "[", "]") + 1;
            }
            if (!at(cursor, "(")) fail(cursor, "expected control-flow condition");
            cursor = pair(cursor, "(", ")") + 1;
            auto child = statement(cursor);
            SdagNode node{kind, tokens_[start].begin, child.end, {std::move(child)}};
            if (kind == SdagKind::If && at(cursor, "else")) {
                ++cursor;
                auto otherwise = statement(cursor);
                node.end = otherwise.end;
                node.children.push_back(std::move(otherwise));
            }
            return node;
        }

        // Opaque C++ statement: skip balanced delimiters rather than examining
        // punctuation or SDAG-like words inside expressions or literals.
        const auto start = cursor;
        while (cursor < tokens_.size() && !at(cursor, ";")) {
            if (at(cursor, "}")) fail(cursor, "expected ';' in ordinary statement");
            if (at(cursor, "(")) cursor = pair(cursor, "(", ")") + 1;
            else if (at(cursor, "[")) cursor = pair(cursor, "[", "]") + 1;
            else if (at(cursor, "{")) cursor = pair(cursor, "{", "}") + 1;
            else ++cursor;
        }
        if (!at(cursor, ";")) fail(cursor, "unterminated ordinary statement");
        const auto end = tokens_[cursor++].end;
        return {SdagKind::Ordinary, tokens_[start].begin, end, {}};
    }

    static bool contains_sdag(const std::vector<SdagNode>& nodes) {
        for (const auto& node : nodes) {
            if (node.kind == SdagKind::When || node.kind == SdagKind::Overlap ||
                node.kind == SdagKind::Case || node.kind == SdagKind::Forall ||
                contains_sdag(node.children))
                return true;
        }
        return false;
    }
};
} // namespace

SdagBody parse_sdag_body(std::string_view body) {
    return SdagParser(body).parse();
}

} // namespace charmsharp::detail

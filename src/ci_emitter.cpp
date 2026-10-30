#include "ci_emitter.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace charmsharp::detail {
namespace {

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    return text;
}

std::string_view slice(std::string_view source, SourceRange range) {
    return source.substr(range.begin, range.end - range.begin);
}

std::string signature(std::string_view source, const EntryDeclaration& entry) {
    if (entry.is_implicit)
        return "void " + entry.name + entry.parameter_list;
    std::string result;
    if (!entry.attributes.empty()) {
        result += "[";
        for (std::size_t i = 0; i < entry.attributes.size(); ++i) {
            if (i != 0) result += ", ";
            result += entry.attributes[i];
        }
        result += "] ";
    }
    result += trim(slice(source, entry.method_signature));
    return result;
}

// Carry original byte offsets through SDAG's textual insertions. The emitted
// interface stays byte-identical; only diagnostic metadata changes.
struct SdagOutput {
    std::string text;
    std::vector<std::size_t> source_offsets;

    void literal(std::string_view value, std::size_t offset) {
        text += value;
        source_offsets.insert(source_offsets.end(), value.size(), offset);
    }

    void copy(std::string_view source, std::size_t begin, std::size_t end) {
        text += source.substr(begin, end - begin);
        for (auto i = begin; i < end; ++i) source_offsets.push_back(i);
    }

    void append(SdagOutput other) {
        text += other.text;
        source_offsets.insert(source_offsets.end(),
                              other.source_offsets.begin(), other.source_offsets.end());
    }
};

SdagOutput emit_sdag_node(std::string_view text, const EntryDeclaration& entry,
                          const SdagNode& node);

SdagOutput emit_sdag_nodes(std::string_view text, const EntryDeclaration& entry,
                           const std::vector<SdagNode>& nodes) {
    SdagOutput result;
    for (std::size_t n = 0; n < nodes.size();) {
        const auto& node = nodes[n];
        if (node.kind == SdagKind::Ordinary) {
            auto last = n;
            while (last + 1 < nodes.size() &&
                   nodes[last + 1].kind == SdagKind::Ordinary)
                ++last;
            result.literal("serial {\n", node.begin);
            result.copy(text, node.begin, nodes[last].end);
            result.literal("\n}\n", nodes[last].end);
            n = last + 1;
            continue;
        }
        result.append(emit_sdag_node(text, entry, node));
        result.literal("\n", node.end);
        ++n;
    }
    return result;
}

SdagOutput emit_sdag_node(std::string_view text, const EntryDeclaration& entry,
                          const SdagNode& node) {
    SdagOutput result;
    if (node.kind == SdagKind::Ordinary) {
        result.literal("serial { ", node.begin);
        result.copy(text, node.begin, node.end);
        result.literal(" }", node.end);
        return result;
    }
    if (node.kind == SdagKind::Serial) {
        result.copy(text, node.begin, node.end);
        return result;
    }
    if (node.kind == SdagKind::Block) {
        result.literal("{\n", node.begin);
        result.append(emit_sdag_nodes(text, entry, node.children));
        result.literal("\n}", node.end - 1);
        return result;
    }
    if (node.kind == SdagKind::If || node.kind == SdagKind::For ||
        node.kind == SdagKind::While || node.kind == SdagKind::Forall ||
        node.kind == SdagKind::Overlap || node.kind == SdagKind::Case) {
        auto cursor = node.begin;
        for (const auto& child : node.children) {
            result.copy(text, cursor, child.begin);
            result.append(emit_sdag_node(text, entry, child));
            cursor = child.end;
        }
        result.copy(text, cursor, node.end);
        return result;
    }
    if (node.kind != SdagKind::When)
        throw std::logic_error("unsupported validated SDAG node");

    const auto match = std::find_if(entry.sdag_receives.begin(), entry.sdag_receives.end(),
        [&](const SdagReceiveLayout& layout) {
            return layout.statement.begin == entry.sdag_body_start + node.begin;
        });
    if (match == entry.sdag_receives.end())
        throw std::logic_error("missing analyzed SDAG receive layout");
    result.copy(text, node.begin, node.end);
    if (!match->explicit_serial) {
        const auto at = match->body_brace_begin - (entry.sdag_body_start + node.begin);
        result.text.insert(at, "serial ");
        result.source_offsets.insert(result.source_offsets.begin() + at, 7,
                                     match->body_brace_begin - entry.sdag_body_start);
    }
    return result;
}

std::string type_kind(const ParallelDeclaration& declaration) {
    switch (declaration.kind) {
        case ParallelKind::Chare: return "chare";
        case ParallelKind::Mainchare: return "mainchare";
        case ParallelKind::Group: return "group";
        case ParallelKind::Nodegroup: return "nodegroup";
        case ParallelKind::Array:
            return "array [" + declaration.array_dimension + "D]";
    }
    throw std::logic_error("unsupported validated parallel kind");
}

} // namespace

std::string emit_ci_body(std::string_view source, const SemanticModel& model,
                         bool include_specialization_extern,
                         std::vector<CiLineOrigin>* origins) {
    std::string ci;
    std::size_t line = 1;
    const auto add = [&](const std::string& value, std::size_t offset,
                         const std::vector<std::size_t>* byte_origins = nullptr) {
        for (std::size_t i = 0; i < value.size(); ++i) {
            if ((i == 0 || value[i - 1] == '\n') && value[i] != '\n' && origins) {
                std::size_t source_offset = offset;
                if (byte_origins && i < byte_origins->size()) {
                    // Prefer the first non-indentation character on this line.
                    auto first = i;
                    while (first < byte_origins->size() &&
                           (value[first] == ' ' || value[first] == '\t')) ++first;
                    if (first < byte_origins->size() && value[first] != '\n')
                        source_offset += (*byte_origins)[first];
                    else
                        source_offset += (*byte_origins)[i];
                }
                origins->push_back({line, source_offset});
            }
            if (value[i] == '\n') ++line;
        }
        ci += value;
    };
    if (include_specialization_extern && !model.template_registrations.empty())
        add("    extern module " + model.module_name + "_specializations;\n", 0);
    for (const auto& pupable : model.pupable_classes)
        add("    PUPable " + pupable.name + ";\n", pupable.attribute.begin);
    for (const auto& message : model.messages) {
        if (message.variable_arrays.empty()) {
            add("    message " + message.name + ";\n", message.name_source.begin);
            continue;
        }
        add("    message " + message.name + " {\n", message.name_source.begin);
        for (const auto& field : message.variable_arrays)
            add("        " + field.type_name + " " + field.name + "[];\n",
                field.name_source.begin);
        add("    };\n", message.name_source.begin);
    }
    for (const auto& readonly : model.readonly_variables)
        add("    readonly " + std::string(readonly.is_message ? "message " : "") +
            std::string(trim(slice(source, readonly.declarator))) + ";\n",
            readonly.declarator.begin);
    for (const auto& hook : model.init_hooks)
        add("    " + std::string(hook.node_level ? "initnode" : "initproc") +
            " void " + hook.name + "();\n", hook.attribute.begin);
    for (const auto& chare : model.parallel_types) {
        if (!chare.template_declaration.empty())
            add("    " + chare.template_declaration + "\n", chare.name_source.begin);
        std::string header = "    " + type_kind(chare) + " " + chare.name;
        if (!chare.parallel_base.empty()) header += " : " + chare.parallel_base;
        add(header + " {\n", chare.name_source.begin);
        for (const auto& entry : chare.entries)
            if (!entry.is_sdag)
                add("        entry " + signature(source, entry) + ";\n",
                    entry.is_implicit ? chare.name_source.begin : entry.method_signature.begin);
        for (const auto& entry : chare.entries) {
            if (!entry.is_sdag) continue;
            if (!entry.sdag_body)
                throw std::logic_error("missing analyzed SDAG body");
            const auto body = source.substr(entry.sdag_body_start);
            const auto lowered = emit_sdag_nodes(body, entry, entry.sdag_body->statements);
            add("        entry " + signature(source, entry) + " {\n",
                entry.method_signature.begin);
            add(lowered.text + "\n", entry.sdag_body_start, &lowered.source_offsets);
            add("};\n", entry.method_signature.begin);
        }
        add("    };\n", chare.name_source.begin);
    }
    return ci;
}

std::string emit_ci(std::string_view source, const SemanticModel& model) {
    return std::string(model.is_mainmodule ? "mainmodule " : "module ") +
           model.module_name + " {\n" + emit_ci_body(source, model, true) + "};\n";
}

std::string emit_registration_ci_body(const SemanticModel& model) {
    std::string body;
    for (const auto& registration : model.template_registrations)
        body += "    chare " + registration.name + registration.arguments + ";\n";
    return body;
}

std::string emit_registration_ci(const SemanticModel& model) {
    if (model.template_registrations.empty()) return {};
    std::string ci = "module " + model.module_name + "_specializations {\n";
    ci += "    extern module " + model.module_name + ";\n";
    ci += emit_registration_ci_body(model);
    ci += "};\n";
    return ci;
}

} // namespace charmsharp::detail

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

std::string emit_sdag_node(std::string_view text, const EntryDeclaration& entry,
                           const SdagNode& node);

std::string emit_sdag_nodes(std::string_view text, const EntryDeclaration& entry,
                            const std::vector<SdagNode>& nodes) {
    std::string result;
    for (std::size_t n = 0; n < nodes.size();) {
        const auto& node = nodes[n];
        if (node.kind == SdagKind::Ordinary) {
            auto last = n;
            while (last + 1 < nodes.size() &&
                   nodes[last + 1].kind == SdagKind::Ordinary)
                ++last;
            result += "serial {\n";
            result += std::string(text.substr(node.begin, nodes[last].end - node.begin));
            result += "\n}\n";
            n = last + 1;
            continue;
        }
        result += emit_sdag_node(text, entry, node);
        result += "\n";
        ++n;
    }
    return result;
}

std::string emit_sdag_node(std::string_view text, const EntryDeclaration& entry,
                           const SdagNode& node) {
    if (node.kind == SdagKind::Ordinary)
        return "serial { " + std::string(text.substr(node.begin, node.end - node.begin)) + " }";
    if (node.kind == SdagKind::Serial)
        return std::string(text.substr(node.begin, node.end - node.begin));
    if (node.kind == SdagKind::Block)
        return "{\n" + emit_sdag_nodes(text, entry, node.children) + "\n}";
    if (node.kind == SdagKind::If || node.kind == SdagKind::For ||
        node.kind == SdagKind::While || node.kind == SdagKind::Forall ||
        node.kind == SdagKind::Overlap || node.kind == SdagKind::Case) {
        std::string lowered;
        auto cursor = node.begin;
        for (const auto& child : node.children) {
            lowered += std::string(text.substr(cursor, child.begin - cursor));
            lowered += emit_sdag_node(text, entry, child);
            cursor = child.end;
        }
        lowered += std::string(text.substr(cursor, node.end - cursor));
        return lowered;
    }
    if (node.kind != SdagKind::When)
        throw std::logic_error("unsupported validated SDAG node");

    const auto match = std::find_if(entry.sdag_receives.begin(), entry.sdag_receives.end(),
        [&](const SdagReceiveLayout& layout) {
            return layout.statement.begin == entry.sdag_body_start + node.begin;
        });
    if (match == entry.sdag_receives.end())
        throw std::logic_error("missing analyzed SDAG receive layout");
    std::string lowered(text.substr(node.begin, node.end - node.begin));
    if (!match->explicit_serial)
        lowered.insert(match->body_brace_begin - (entry.sdag_body_start + node.begin),
                       "serial ");
    return lowered;
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

std::string emit_ci(std::string_view source, const SemanticModel& model) {
    std::string ci = std::string(model.is_mainmodule ? "mainmodule " : "module ") +
                     model.module_name + " {\n";
    if (!model.template_registrations.empty())
        ci += "    extern module " + model.module_name + "_specializations;\n";
    for (const auto& message : model.messages) {
        if (message.variable_arrays.empty()) {
            ci += "    message " + message.name + ";\n";
            continue;
        }
        ci += "    message " + message.name + " {\n";
        for (const auto& field : message.variable_arrays)
            ci += "        " + field.type_name + " " + field.name + "[];\n";
        ci += "    };\n";
    }
    for (const auto& readonly : model.readonly_variables)
        ci += "    readonly " + std::string(trim(slice(source, readonly.declarator))) + ";\n";
    for (const auto& chare : model.parallel_types) {
        if (!chare.template_declaration.empty())
            ci += "    " + chare.template_declaration + "\n";
        ci += "    " + type_kind(chare) + " " + chare.name;
        if (!chare.parallel_base.empty()) ci += " : " + chare.parallel_base;
        ci += " {\n";
        // Charmxi registration entries are emitted before SDAG definitions,
        // independent of their order in the original Charm# class body.
        for (const auto& entry : chare.entries)
            if (!entry.is_sdag)
                ci += "        entry " + signature(source, entry) + ";\n";
        for (const auto& entry : chare.entries) {
            if (!entry.is_sdag) continue;
            if (!entry.sdag_body)
                throw std::logic_error("missing analyzed SDAG body");
            const auto body = source.substr(entry.sdag_body_start);
            const auto lowered = emit_sdag_nodes(body, entry, entry.sdag_body->statements);
            ci += "        entry " + signature(source, entry) + " {\n" +
                  lowered + "\n};\n";
        }
        ci += "    };\n";
    }
    ci += "};\n";
    return ci;
}

std::string emit_registration_ci(const SemanticModel& model) {
    if (model.template_registrations.empty()) return {};
    std::string ci = "module " + model.module_name + "_specializations {\n";
    ci += "    extern module " + model.module_name + ";\n";
    for (const auto& registration : model.template_registrations)
        ci += "    chare " + registration.name + registration.arguments + ";\n";
    ci += "};\n";
    return ci;
}

} // namespace charmsharp::detail

#include "cpp_emitter.hpp"

#include <algorithm>
#include <utility>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace charmsharp::detail {
namespace {

struct Edit {
    std::size_t begin;
    std::size_t end;
    std::string replacement;
};

void replace_parallel(std::vector<Edit>& edits, const ParallelDeclaration& chare) {
    edits.push_back({chare.source.begin, chare.name_source.begin, "class "});
    edits.push_back({chare.name_source.end, chare.opening_brace,
                     " : public CBase_" + chare.name + chare.template_arguments +
                     (chare.ordinary_bases.empty() ? " " : ", " + chare.ordinary_bases + " ")});
    for (const auto& label : chare.entry_labels)
        edits.push_back({label.begin, label.end, "public"});
    for (const auto& entry : chare.entries) {
        if (entry.is_implicit) continue;
        for (const auto& attribute : entry.attribute_ranges)
            edits.push_back({attribute.begin, attribute.end, ""});
        // A receive declaration is provided by SDAG; an SDAG entry body is
        // lowered into the .ci backend and likewise has no C++ definition.
        if (entry.is_receive || entry.is_sdag)
            edits.push_back({entry.method_signature.begin, entry.source.end, ""});
    }
    if (std::any_of(chare.entries.begin(), chare.entries.end(),
                    [](const EntryDeclaration& entry) { return entry.is_sdag; }))
        edits.push_back({chare.closing_brace, chare.closing_brace,
                         "\npublic:\n    " + chare.name + "_SDAG_CODE\n"});
}

std::size_t template_start(std::string_view source, const ParallelDeclaration& chare) {
    // The keyword source range excludes the preceding C++ template prefix.
    const auto pos = source.rfind(chare.template_declaration, chare.source.begin);
    if (pos == std::string_view::npos ||
        pos + chare.template_declaration.size() > chare.source.begin)
        throw std::logic_error("missing chare template prefix");
    return pos;
}

std::string rewrite(std::string text, std::vector<Edit> edits, std::size_t base) {
    std::stable_sort(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) {
        return a.begin > b.begin;
    });
    auto boundary = base + text.size();
    for (const auto& edit : edits) {
        if (edit.begin < base || edit.begin > edit.end || edit.end > boundary)
            throw std::logic_error("overlapping or invalid C++ source edits");
        text.replace(edit.begin - base, edit.end - edit.begin, edit.replacement);
        boundary = edit.begin;
    }
    return text;
}

} // namespace

std::string emit_template_header(std::string_view source, const SemanticModel& model) {
    std::string header;
    for (const auto& chare : model.parallel_types) {
        if (chare.template_declaration.empty()) continue;
        if (header.empty())
            header = "#pragma once\n#include \"" + model.module_name + ".decl.h\"\n\n";
        const auto begin = template_start(source, chare);
        std::vector<Edit> edits;
        replace_parallel(edits, chare);
        for (const auto& parameter : model.marshalled_arrays) {
            if (parameter.name.begin < begin || parameter.suffix.end > chare.source.end)
                continue;
            if (std::any_of(edits.begin(), edits.end(), [&](const Edit& edit) {
                    return edit.replacement.empty() &&
                           edit.begin <= parameter.name.begin &&
                           parameter.suffix.end <= edit.end;
                })) continue;
            edits.push_back({parameter.name.begin, parameter.name.begin, "*"});
            edits.push_back({parameter.suffix.begin, parameter.suffix.end, ""});
        }
        header += rewrite(std::string(source.substr(begin, chare.source.end - begin)),
                          std::move(edits), begin);
        header += "\n\n";
    }
    if (!header.empty()) {
        header += "#define CK_TEMPLATES_ONLY\n#include \"" + model.module_name +
                  ".def.h\"\n#undef CK_TEMPLATES_ONLY\n";
    }
    return header;
}

std::string emit_cpp(std::string_view source, const SemanticModel& model) {
    std::vector<Edit> edits;
    for (const auto& registration : model.template_registrations)
        edits.push_back({registration.source.begin, registration.source.end, ""});
    for (const auto& readonly : model.readonly_variables)
        edits.push_back({readonly.source.begin, readonly.declarator.begin, ""});
    for (const auto& message : model.messages) {
        edits.push_back({message.source.begin, message.name_source.begin, "class "});
        edits.push_back({message.name_source.end, message.opening_brace,
                         " : public CMessage_" + message.name +
                         (message.ordinary_bases.empty() ? " " : ", " + message.ordinary_bases + " ")});
        if (!message.variable_arrays.empty()) {
            // Charmxi's generated pack/unpack code must access these pointers.
            edits.push_back({message.opening_brace + 1, message.opening_brace + 1,
                             "\n    friend class CMessage_" + message.name + ";"});
            for (const auto& field : message.variable_arrays) {
                edits.push_back({field.name_source.begin, field.name_source.begin, "*"});
                edits.push_back({field.suffix.begin, field.suffix.end, ""});
            }
        }
    }
    bool emitted_template_header = false;
    for (const auto& chare : model.parallel_types) {
        if (chare.template_declaration.empty()) {
            replace_parallel(edits, chare);
        } else {
            const auto start = template_start(source, chare);
            edits.push_back({start, chare.source.end,
                emitted_template_header ? "" :
                    "#include \"" + model.module_name + "_templates.h\"\n"});
            emitted_template_header = true;
        }
    }
    for (const auto& parameter : model.marshalled_arrays) {
        // Template class definitions were moved into the shared header.
        if (std::any_of(model.parallel_types.begin(), model.parallel_types.end(),
                        [&](const ParallelDeclaration& chare) {
                            return !chare.template_declaration.empty() &&
                                   parameter.name.begin >= template_start(source, chare) &&
                                   parameter.suffix.end <= chare.source.end;
                        })) continue;
        // SDAG owns these signatures. Do not rewrite bytes that are removed
        // with the containing method or explicit receive declaration.
        if (std::any_of(edits.begin(), edits.end(), [&](const Edit& edit) {
                return edit.replacement.empty() &&
                       edit.begin <= parameter.name.begin &&
                       parameter.suffix.end <= edit.end;
            }))
            continue;
        edits.push_back({parameter.name.begin, parameter.name.begin, "*"});
        edits.push_back({parameter.suffix.begin, parameter.suffix.end, ""});
    }

    std::string cpp = rewrite(std::string(source), std::move(edits), 0);
    cpp.insert(0, "#include \"" + model.module_name + ".decl.h\"\n");
    if (cpp.empty() || cpp.back() != '\n') cpp += '\n';
    if (!model.template_registrations.empty()) {
        cpp += "#include \"" + model.module_name + "_specializations.decl.h\"\n";
    }
    cpp += "#include \"" + model.module_name + ".def.h\"\n";
    if (!model.template_registrations.empty())
        cpp += "#include \"" + model.module_name + "_specializations.def.h\"\n";
    return cpp;
}

} // namespace charmsharp::detail


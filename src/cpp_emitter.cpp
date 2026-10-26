#include "cpp_emitter.hpp"

#include <algorithm>
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
                     " : public CBase_" + chare.name +
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

} // namespace

std::string emit_cpp(std::string_view source, const SemanticModel& model) {
    std::vector<Edit> edits;
    for (const auto& readonly : model.readonly_variables)
        edits.push_back({readonly.source.begin, readonly.declarator.begin, ""});
    for (const auto& message : model.messages) {
        edits.push_back({message.source.begin, message.name_source.begin, "class "});
        edits.push_back({message.name_source.end, message.opening_brace,
                         " : public CMessage_" + message.name +
                         (message.ordinary_bases.empty() ? " " : ", " + message.ordinary_bases + " ")});
    }
    for (const auto& chare : model.parallel_types)
        replace_parallel(edits, chare);
    for (const auto& parameter : model.marshalled_arrays) {
        edits.push_back({parameter.name.begin, parameter.name.begin, "*"});
        edits.push_back({parameter.suffix.begin, parameter.suffix.end, ""});
    }

    // Apply edits from right to left so the source ranges remain stable.
    std::stable_sort(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) {
        return a.begin > b.begin;
    });
    std::string cpp(source);
    for (const auto& edit : edits)
        cpp.replace(edit.begin, edit.end - edit.begin, edit.replacement);
    cpp.insert(0, "#include \"" + model.module_name + ".decl.h\"\n");
    if (cpp.empty() || cpp.back() != '\n') cpp += '\n';
    cpp += "#include \"" + model.module_name + ".def.h\"\n";
    return cpp;
}

} // namespace charmsharp::detail

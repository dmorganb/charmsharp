

namespace charmsharp {

MultiGenerated translate_many(const std::vector<std::string_view>& sources,
                              std::string_view module_name,
                              const std::vector<std::string_view>& source_paths) {
    if (sources.empty()) throw Diagnostic("expected at least one input", 0);
    if (!source_paths.empty() && source_paths.size() != sources.size())
        throw Diagnostic("source path count must match input count", 0);
    MultiGenerated result;
    std::string body;
    std::string specializations;
    bool has_templates = false;
    std::size_t mains = 0;
    struct LocatedName { std::string name; std::size_t file; std::size_t offset; };
    std::vector<LocatedName> declared_names;
    std::size_t file_index = 0, main_file = 0, main_offset = 0;
    std::string header = "#pragma once\n#include <memory>\n";
    std::string pup_forwards;
    std::string user_includes;
    for (auto source : sources) {
        detail::SemanticModel model;
        try {
            model = detail::analyze(source, module_name);
        } catch (const Diagnostic& error) {
            throw MultiDiagnostic(error.what(), file_index, error.offset());
        }
        const auto register_name = [&](const std::string& name, std::size_t offset) {
            const auto prior = std::find_if(declared_names.begin(), declared_names.end(),
                [&](const LocatedName& item) { return item.name == name; });
            if (prior != declared_names.end())
                throw MultiDiagnostic("duplicate Charm# declaration across input files: " + name,
                                      file_index, offset, prior->file, prior->offset);
            declared_names.push_back({name, file_index, offset});
        };
        for (const auto& chare : model.parallel_types)
            register_name(chare.name, chare.name_source.begin);
        for (const auto& message : model.messages)
            register_name(message.name, message.name_source.begin);
        for (const auto& readonly : model.readonly_variables)
            register_name(readonly.name, readonly.declarator.begin);
        for (const auto& pupable : model.pupable_classes)
            pup_forwards += "class " + pupable.name + ";\n";

        std::vector<detail::CiLineOrigin> ci_origins;
        const auto ci_base_line = 1 + std::count(body.begin(), body.end(), '\n');
        const auto ci_piece = detail::emit_ci_body(source, model, false, &ci_origins);
        if (!source_paths.empty()) {
            for (const auto& origin : ci_origins) {
                const auto bounded = std::min(origin.source_offset, source.size());
                result.interface_locations.push_back({
                    ci_base_line + origin.generated_line,
                    std::string(source_paths[file_index]),
                    1 + static_cast<std::size_t>(
                        std::count(source.begin(), source.begin() + bounded, '\n'))
                });
            }
        }
        body += ci_piece;
        specializations += detail::emit_registration_ci_body(model);
        if (std::any_of(model.parallel_types.begin(), model.parallel_types.end(),
                [](const detail::ParallelDeclaration& chare) {
                    return !chare.template_declaration.empty();
                })) has_templates = true;
        if (model.is_mainmodule) {
            std::size_t offset = 0;
            for (const auto& chare : model.parallel_types)
                if (chare.kind == detail::ParallelKind::Mainchare) {
                    offset = chare.name_source.begin;
                    break;
                }
            if (mains != 0)
                throw MultiDiagnostic("multiple mainchares across input files",
                                      file_index, offset, main_file, main_offset);
            ++mains;
            main_file = file_index;
            main_offset = offset;
        }

        // Preserve user includes in the shared class header; class definitions
        // may depend on types declared by ordinary project headers.
        std::size_t line_begin = 0;
        while (line_begin < source.size()) {
            const auto line_end = source.find('\n', line_begin);
            const auto line = source.substr(line_begin,
                line_end == std::string_view::npos ? source.size() - line_begin :
                                                     line_end - line_begin);
            const auto first = line.find_first_not_of(" \t");
            if (first != std::string_view::npos &&
                line.substr(first, 8) == "#include")
                user_includes += std::string(line) + "\n";
            if (line_end == std::string_view::npos) break;
            line_begin = line_end + 1;
        }

        header += detail::emit_shared_classes(source, model,
            source_paths.empty() ? std::string_view{} : source_paths[file_index]);
        result.cpp_sources.push_back(detail::emit_multi_cpp(source, model));
        ++file_index;
    }
    if (!specializations.empty()) {
        body = "    extern module " + std::string(module_name) +
               "_specializations;\n" + body;
        for (auto& origin : result.interface_locations) ++origin.generated_line;
    }
    result.interface_source = std::string(mains == 1 ? "mainmodule " : "module ") +
                              std::string(module_name) + " {\n" + body + "};\n";
    if (has_templates)
        header += "#define CK_TEMPLATES_ONLY\n#include \"" +
            std::string(module_name) + ".def.h\"\n#undef CK_TEMPLATES_ONLY\n";
    const auto preamble_end = header.find("\n", header.find("\n") + 1) + 1;
    header.insert(preamble_end, user_includes + pup_forwards +
                  "#include \"" + std::string(module_name) + ".decl.h\"\n");
    result.shared_header_source = std::move(header);
    if (!specializations.empty())
        result.specialization_interface_source =
            "module " + std::string(module_name) + "_specializations {\n" +
            "    extern module " + std::string(module_name) + ";\n" +
            specializations + "};\n";
    result.registration_cpp_source = "#include \"" + std::string(module_name) +
        "_classes.h\"\n";
    if (!specializations.empty())
        result.registration_cpp_source += "#include \"" + std::string(module_name) +
            "_specializations.decl.h\"\n";
    result.registration_cpp_source += "#include \"" + std::string(module_name) +
        ".def.h\"\n";
    if (!specializations.empty())
        result.registration_cpp_source += "#include \"" + std::string(module_name) +
            "_specializations.def.h\"\n";
    return result;
}
} // namespace charmsharp
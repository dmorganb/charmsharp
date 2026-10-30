#include "charmsharp/translator.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

void usage() {
    std::cerr << "Usage: charmsharp [--module NAME] [--out-dir DIR] [--force] [--build [-o EXECUTABLE]] FILE.csh [OTHER.csh ...] [NATIVE.cpp ...]\n";
}

// Execute arguments directly: paths and module names never pass through a shell.
void run_charmc(const fs::path& directory, std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("charmc"));
    for (auto& arg : args) argv.push_back(arg.data());
    argv.push_back(nullptr);
    const pid_t child = fork();
    if (child < 0) throw std::runtime_error("cannot start charmc: " + std::string(std::strerror(errno)));
    if (child == 0) {
        if (chdir(directory.c_str()) != 0) {
            std::perror("charmsharp: cannot enter output directory");
            _exit(127);
        }
        execvp(argv[0], argv.data());
        std::perror("charmsharp: cannot execute charmc (is it on PATH?)");
        _exit(127);
    }
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) throw std::runtime_error("cannot wait for charmc");
    }
    if (WIFSIGNALED(status))
        throw std::runtime_error("charmc terminated by signal " + std::to_string(WTERMSIG(status)));
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        throw std::runtime_error("charmc failed with exit status " + std::to_string(WEXITSTATUS(status)));
}

std::string cpp_line_directive(const fs::path& path) {
    std::string escaped;
    for (const char c : fs::absolute(path).string()) {
        if (c == '\\' || c == '"') escaped += '\\';
        escaped += c;
    }
    return "#line 1 \"" + escaped + "\"\n";
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_file(const fs::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write " + path.string());
    output << content;
    if (!output) throw std::runtime_error("write failed for " + path.string());
}

// Preserve timestamps for unchanged generated artifacts. This allows Make to
// avoid rebuilding downstream objects when only unrelated C++ bodies change.
void write_if_changed(const fs::path& path, const std::string& content) {
    if (fs::exists(path) && read_file(path) == content) return;
    write_file(path, content);
}

} // namespace

int main(int argc, char** argv) {
    std::vector<fs::path> input_paths;
    fs::path output_dir = ".";
    std::string module;
    bool force = false;
    bool build = false;
    fs::path executable;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--module" || arg == "--out-dir" || arg == "-o") && i + 1 < argc) {
            if (arg == "--module") module = argv[++i];
            else if (arg == "--out-dir") output_dir = argv[++i];
            else executable = argv[++i];
        } else if (arg == "--build") {
            build = true;
        } else if (arg == "--force") {
            force = true;
        } else if (arg == "--help") {
            usage();
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            input_paths.emplace_back(arg);
        } else {
            usage();
            return 2;
        }
    }
    std::vector<fs::path> native_paths;
    for (auto it = input_paths.begin(); it != input_paths.end();) {
        if (it->extension() == ".cpp" || it->extension() == ".cc" ||
            it->extension() == ".C") {
            native_paths.push_back(*it);
            it = input_paths.erase(it);
        } else {
            ++it;
        }
    }
    if (input_paths.empty() || (!build && !executable.empty())) {
        usage();
        return 2;
    }
    if (module.empty()) module = input_paths.front().stem().string();

    try {
        if (build) {
            executable = fs::absolute(executable.empty() ? fs::path(module) : executable);
            if (!force && fs::exists(executable))
                throw std::runtime_error("executable exists; use --force to overwrite");
        }
        if (input_paths.size() > 1 || !native_paths.empty()) {
            // Project path: translate each Charm# unit, generate shared declarations
            // and one registration translation unit, then compile/link independently.
            std::vector<std::string> source_texts;
            std::vector<std::string_view> views;
            for (const auto& path : input_paths) source_texts.push_back(read_file(path));
            for (const auto& source : source_texts) views.push_back(source);
            charmsharp::MultiGenerated combined;
            try {
                combined = charmsharp::translate_many(views, module);
            } catch (const charmsharp::MultiDiagnostic& error) {
                const auto display = [&](std::size_t index, std::size_t offset,
                                         const std::string& message) {
                    std::cerr << input_paths.at(index).string() << ':'
                              << charmsharp::format_diagnostic(
                                    source_texts.at(index), charmsharp::Diagnostic(message, offset))
                              << '\n' << charmsharp::format_source_excerpt(
                                    source_texts.at(index), offset) << '\n';
                };
                display(error.file(), error.offset(), error.what());
                if (error.has_previous())
                    display(error.previous_file(), error.previous_offset(),
                            "note: previous declaration is here");
                return 1;
            }
            const auto ci = output_dir / (module + ".ci");
            const auto header = output_dir / (module + "_classes.h");
            const auto registration = output_dir / (module + "_registry.cpp");
            std::vector<fs::path> outputs{ci, header, registration};
            const auto specialization = output_dir / (module + "_specializations.ci");
            if (!combined.specialization_interface_source.empty())
                outputs.push_back(specialization);
            const std::size_t cpp_start = outputs.size();
            for (const auto& path : input_paths) {
                if (path.extension() != ".csh")
                    throw std::runtime_error("expected Charm# .csh input: " + path.string());
                const auto cpp = output_dir / (path.stem().string() + ".cpp");
                if (std::find(outputs.begin(), outputs.end(), cpp) != outputs.end())
                    throw std::runtime_error("duplicate multi-file output basename: " + cpp.string());
                outputs.push_back(cpp);
            }
            for (const auto& path : native_paths) {
                const auto object = path.stem().string() + "_native.o";
                const auto native_cpp = output_dir / (path.stem().string() + ".cpp");
                if (std::find(outputs.begin(), outputs.end(), native_cpp) != outputs.end())
                    throw std::runtime_error("native source collides with generated .cpp basename: " +
                                             path.string());
                if (std::any_of(input_paths.begin(), input_paths.end(),
                    [&](const fs::path& source) {
                        return source.stem().string() + ".o" == object;
                    }))
                    throw std::runtime_error("native object collides with generated object: " +
                                             path.string());
            }
            if (!force)
                for (const auto& output : outputs)
                    if (fs::exists(output))
                        throw std::runtime_error("output exists; use --force to overwrite");
            fs::create_directories(output_dir);
            write_if_changed(ci, combined.interface_source);
            write_if_changed(header, combined.shared_header_source);
            write_if_changed(registration, combined.registration_cpp_source);
            if (!combined.specialization_interface_source.empty())
                write_if_changed(specialization, combined.specialization_interface_source);
            for (std::size_t i = 0; i < input_paths.size(); ++i) {
                auto cpp_source = combined.cpp_sources[i];
                // The emitter preserves line counts for removed Charm# declarations.
                // Put the mapping after the generated shared-header include.
                const auto header_end = cpp_source.find('\n');
                if (header_end != std::string::npos)
                    cpp_source.insert(header_end + 1, cpp_line_directive(input_paths[i]));
                write_if_changed(outputs[i + cpp_start], cpp_source);
            }
            for (const auto& output : outputs) std::cout << output << '\n';
            if (build) {
                std::cout.flush();
                run_charmc(output_dir, {module + ".ci"});
                if (!combined.specialization_interface_source.empty())
                    run_charmc(output_dir, {module + "_specializations.ci"});
                std::vector<std::string> objects;
                for (std::size_t i = 0; i < input_paths.size(); ++i) {
                    const auto object = input_paths[i].stem().string() + ".o";
                    std::vector<std::string> args{"-c", outputs[i + cpp_start].filename().string(), "-o", object};
                    for (const auto& path : input_paths)
                        args.push_back("-I" + fs::absolute(path).parent_path().string());
                    run_charmc(output_dir, std::move(args));
                    objects.push_back(object);
                }
                const auto registry_object = module + "_registry.o";
                std::vector<std::string> args{"-c", registration.filename().string(), "-o", registry_object};
                for (const auto& path : input_paths)
                    args.push_back("-I" + fs::absolute(path).parent_path().string());
                run_charmc(output_dir, std::move(args));
                objects.push_back(registry_object);
                for (const auto& path : native_paths) {
                    const auto object = path.stem().string() + "_native.o";
                    std::vector<std::string> native_args{
                        "-c", fs::absolute(path).string(), "-o", object};
                    native_args.push_back("-I" + fs::absolute(path).parent_path().string());
                    run_charmc(output_dir, std::move(native_args));
                    objects.push_back(object);
                }
                args = {"-language", "charm++", "-o", executable.string()};
                args.insert(args.end(), objects.begin(), objects.end());
                run_charmc(output_dir, std::move(args));
                std::cout << executable << '\n';
            }
            return 0;
        }
        const auto source = read_file(input_paths.front());
        charmsharp::Generated generated;
        try {
            generated = charmsharp::translate(source, module);
        } catch (const charmsharp::Diagnostic& error) {
            std::cerr << input_paths.front().string() << ':'
                      << charmsharp::format_diagnostic(source, error) << '\n';
            return 1;
        }

        const auto ci_path = output_dir / (module + ".ci");
        const auto cpp_path = output_dir / (module + ".cpp");
        const auto registration_path = output_dir / (module + "_specializations.ci");
        const auto template_path = output_dir / (module + "_templates.h");
        if (!force && (fs::exists(ci_path) || fs::exists(cpp_path) ||
                       (!generated.registration_interface_source.empty() && fs::exists(registration_path)) ||
                       (!generated.template_header_source.empty() && fs::exists(template_path)))) {
            throw std::runtime_error("output exists; use --force to overwrite");
        }
        fs::create_directories(output_dir);
        write_file(ci_path, generated.interface_source);
        write_file(cpp_path, generated.cpp_source);
        if (!generated.template_header_source.empty())
            write_file(template_path, generated.template_header_source);
        if (!generated.registration_interface_source.empty())
            write_file(registration_path, generated.registration_interface_source);
        std::cout << ci_path << '\n' << cpp_path << '\n';
        if (!generated.registration_interface_source.empty())
            std::cout << registration_path << '\n';
        if (!generated.template_header_source.empty())
            std::cout << template_path << '\n';
        if (build) {
            std::cout.flush();
            run_charmc(output_dir, {module + ".ci"});
            if (!generated.registration_interface_source.empty())
                run_charmc(output_dir, {module + "_specializations.ci"});
            run_charmc(output_dir, {module + ".cpp", "-I" + fs::absolute(input_paths.front()).parent_path().string(),
                                    "-o", executable.string()});
            std::cout << executable << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "charmsharp: " << error.what() << '\n';
        return 1;
    }
    return 0;
}



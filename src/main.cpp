#include "charmsharp/translator.hpp"

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
    std::cerr << "Usage: charmsharp [--module NAME] [--out-dir DIR] [--force] [--build [-o EXECUTABLE]] FILE.csh\n";
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

} // namespace

int main(int argc, char** argv) {
    fs::path input_path;
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
        } else if (!arg.empty() && arg[0] != '-' && input_path.empty()) {
            input_path = arg;
        } else {
            usage();
            return 2;
        }
    }
    if (input_path.empty() || (!build && !executable.empty())) {
        usage();
        return 2;
    }
    if (module.empty()) module = input_path.stem().string();

    try {
        if (build) {
            executable = fs::absolute(executable.empty() ? fs::path(module) : executable);
            if (!force && fs::exists(executable))
                throw std::runtime_error("executable exists; use --force to overwrite");
        }
        const auto source = read_file(input_path);
        charmsharp::Generated generated;
        try {
            generated = charmsharp::translate(source, module);
        } catch (const charmsharp::Diagnostic& error) {
            std::cerr << input_path.string() << ':'
                      << charmsharp::format_diagnostic(source, error) << '\n';
            return 1;
        }

        const auto ci_path = output_dir / (module + ".ci");
        const auto cpp_path = output_dir / (module + ".cpp");
        if (!force && (fs::exists(ci_path) || fs::exists(cpp_path))) {
            throw std::runtime_error("output exists; use --force to overwrite");
        }
        fs::create_directories(output_dir);
        write_file(ci_path, generated.interface_source);
        write_file(cpp_path, generated.cpp_source);
        std::cout << ci_path << '\n' << cpp_path << '\n';
        if (build) {
            std::cout.flush();
            run_charmc(output_dir, {module + ".ci"});
            run_charmc(output_dir, {module + ".cpp", "-I" + fs::absolute(input_path).parent_path().string(),
                                    "-o", executable.string()});
            std::cout << executable << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "charmsharp: " << error.what() << '\n';
        return 1;
    }
    return 0;
}



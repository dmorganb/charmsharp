#include "charmsharp/translator.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

void usage() {
    std::cerr << "Usage: charmsharp [--module NAME] [--out-dir DIR] [--force] FILE.csh\n";
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
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--module" || arg == "--out-dir") && i + 1 < argc) {
            if (arg == "--module") module = argv[++i];
            else output_dir = argv[++i];
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
    if (input_path.empty()) {
        usage();
        return 2;
    }
    if (module.empty()) module = input_path.stem().string();

    try {
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
    } catch (const std::exception& error) {
        std::cerr << "charmsharp: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

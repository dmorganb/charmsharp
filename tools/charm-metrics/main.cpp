#include "metrics.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using charm_metrics::Counts;
using charm_metrics::Language;
using charm_metrics::Metrics;

namespace {
struct Report {
    std::string name;
    std::string language;
    std::vector<std::string> files;
    Counts counts;
    Metrics metrics;
};

bool supported(const fs::path& p, Language language) {
    auto ext = p.extension().string();
    auto name = p.filename().string();
    if (name.ends_with(".decl.h") || name.ends_with(".def.h")) return false;
    if (language == Language::CharmSharp) return ext == ".csh";
    return ext == ".ci" || ext == ".C" || ext == ".cpp" || ext == ".cc" ||
           ext == ".cxx" || ext == ".h" || ext == ".hpp";
}
std::string read_file(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open: " + path.string());
    std::ostringstream out;
    out << f.rdbuf();
    return out.str();
}
Report measure(std::string name, const fs::path& dir, Language lang) {
    if (!fs::is_directory(dir)) throw std::runtime_error("not a directory: " + dir.string());
    Report r;
    r.name = std::move(name);
    r.language = lang == Language::CharmSharp ? "charmsharp" : "charmpp";
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && supported(entry.path(), lang))
            r.files.push_back(entry.path().generic_string());
    }
    std::sort(r.files.begin(), r.files.end());
    if (r.files.empty()) throw std::runtime_error("no relevant source files: " + dir.string());
    for (const auto& file : r.files) {
        try { r.counts.add(charm_metrics::lex(read_file(file), lang)); }
        catch (const std::exception& e) { throw std::runtime_error(file + ": " + e.what()); }
    }
    r.metrics = charm_metrics::calculate(r.counts);
    return r;
}
void json_string(std::ostream& out, std::string_view s) {
    out << '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) {
                const char* digits = "0123456789abcdef";
                out << "\\u00" << digits[c >> 4] << digits[c & 15];
            } else out << static_cast<char>(c);
        }
    }
    out << '"';
}
void json_inventory(std::ostream& out, const std::map<std::string, std::uint64_t>& items) {
    out << '{';
    bool first = true;
    for (const auto& [name, frequency] : items) {
        if (!first) out << ',';
        first = false;
        json_string(out, name); out << ':' << frequency;
    }
    out << '}';
}
void json_report(std::ostream& out, const Report& r) {
    const auto& m = r.metrics;
    out << "{\"name\":"; json_string(out, r.name);
    out << ",\"language\":"; json_string(out, r.language);
    out << ",\"policy\":\"0.1\",\"files\":[";
    for (std::size_t i = 0; i < r.files.size(); ++i) {
        if (i) out << ',';
        json_string(out, r.files[i]);
    }
    out << "],\"n1\":" << m.n1 << ",\"n2\":" << m.n2
        << ",\"N1\":" << m.N1 << ",\"N2\":" << m.N2
        << ",\"vocabulary\":" << m.vocabulary
        << ",\"length\":" << m.length << ",\"volume\":" << m.volume
        << ",\"difficulty\":" << m.difficulty << ",\"effort\":" << m.effort
        << ",\"time_seconds\":" << m.time_seconds
        << ",\"estimated_bugs\":" << m.estimated_bugs
        << ",\"operators\":";
    json_inventory(out, r.counts.operators);
    out << ",\"operands\":";
    json_inventory(out, r.counts.operands);
    out << '}';
}
double change(double cpp, double sharp) {
    return cpp == 0 ? 0 : (sharp - cpp) * 100.0 / cpp;
}
struct Pair { Report cpp; Report sharp; };
void output(const std::vector<Pair>& pairs, std::string format) {
    std::cout << std::setprecision(12);
    if (format == "json") {
        std::cout << "{\"policy\":\"0.1\",\"pairs\":[";
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            if (i) std::cout << ',';
            std::cout << "{\"benchmark\":";
            json_string(std::cout, pairs[i].cpp.name);
            std::cout << ",\"charmpp\":";
            json_report(std::cout, pairs[i].cpp);
            std::cout << ",\"charmsharp\":";
            json_report(std::cout, pairs[i].sharp);
            std::cout << ",\"volume_change_pct\":" << change(pairs[i].cpp.metrics.volume, pairs[i].sharp.metrics.volume)
                      << ",\"difficulty_change_pct\":" << change(pairs[i].cpp.metrics.difficulty, pairs[i].sharp.metrics.difficulty)
                      << ",\"effort_change_pct\":" << change(pairs[i].cpp.metrics.effort, pairs[i].sharp.metrics.effort)
                      << '}';
        }
        std::cout << "]}\n";
    } else if (format == "csv") {
        std::cout << "benchmark,language,n1,n2,N1,N2,vocabulary,length,volume,difficulty,effort,time_seconds,estimated_bugs\n";
        for (const auto& p : pairs) for (const auto* r : {&p.cpp, &p.sharp}) {
            const auto& m = r->metrics;
            std::cout << r->name << ',' << r->language << ',' << m.n1 << ',' << m.n2
                      << ',' << m.N1 << ',' << m.N2 << ',' << m.vocabulary << ',' << m.length
                      << ',' << m.volume << ',' << m.difficulty << ',' << m.effort
                      << ',' << m.time_seconds << ',' << m.estimated_bugs << '\n';
        }
    } else if (format == "text") {
        for (const auto& p : pairs) {
            std::cout << p.cpp.name << "\n";
            std::cout << "  Charm++ : V=" << p.cpp.metrics.volume
                      << " D=" << p.cpp.metrics.difficulty << " E=" << p.cpp.metrics.effort << '\n';
            std::cout << "  Charm#  : V=" << p.sharp.metrics.volume
                      << " D=" << p.sharp.metrics.difficulty << " E=" << p.sharp.metrics.effort << '\n';
            std::cout << "  Change  : V=" << change(p.cpp.metrics.volume, p.sharp.metrics.volume)
                      << "% D=" << change(p.cpp.metrics.difficulty, p.sharp.metrics.difficulty)
                      << "% E=" << change(p.cpp.metrics.effort, p.sharp.metrics.effort) << "%\n";
        }
    } else throw std::runtime_error("unsupported format: " + format);
}
std::string option(int argc, char** argv, std::string_view key, std::string fallback = "") {
    for (int i = 2; i < argc; ++i) {
        if (argv[i] == key && i+1 < argc) return argv[i+1];
    }
    return fallback;
}
}
int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::runtime_error("usage: charm-metrics benchmark [--root benchmarks] [--format json|csv|text] OR compare --charmpp-dir DIR --charmsharp-dir DIR");
        std::string command = argv[1];
        std::string format = option(argc, argv, "--format", "text");
        std::vector<Pair> pairs;
        if (command == "benchmark") {
            fs::path root = option(argc, argv, "--root", "benchmarks");
            for (const auto& entry : fs::directory_iterator(root)) {
                if (!entry.is_directory()) continue;
                auto a = entry.path() / "charmpp";
                auto b = entry.path() / "charmsharp";
                if (fs::is_directory(a) && fs::is_directory(b)) {
                    auto name = entry.path().filename().string();
                    pairs.push_back({measure(name, a, Language::CharmInterface),
                                     measure(name, b, Language::CharmSharp)});
                }
            }
            std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) {
                return a.cpp.name < b.cpp.name;
            });
            if (pairs.empty()) throw std::runtime_error("no paired benchmarks found");
        } else if (command == "compare") {
            auto a = option(argc, argv, "--charmpp-dir");
            auto b = option(argc, argv, "--charmsharp-dir");
            if (a.empty() || b.empty()) throw std::runtime_error("compare requires both directory options");
            pairs.push_back({measure("comparison", a, Language::CharmInterface),
                             measure("comparison", b, Language::CharmSharp)});
        } else throw std::runtime_error("unknown command: " + command);
        output(pairs, format);
    } catch (const std::exception& e) {
        std::cerr << "charm-metrics: " << e.what() << '\n';
        return 1;
    }
}

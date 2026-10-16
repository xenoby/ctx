#include "swift_plugin.hpp"
#include <cstring>
#include "../core/utils.hpp"

bool SwiftPlugin::supports(const fs::path& path) const {
    return to_lower(path.extension().string()) == ".swift";
}

bool SwiftPlugin::is_interface_file(const fs::path& /*path*/) const {
    return false;
}

std::vector<std::string> SwiftPlugin::parse_imports(const std::string& content) const {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string raw;

    while (std::getline(stream, raw)) {
        std::string line = trim(normalize_ws(raw));
        if (line.empty() || line.rfind("//", 0) == 0) continue;

        // import Foundation
        // import class Module.Type
        // import struct Module.Type
        if (line.rfind("import ", 0) != 0) continue;
        std::string rest = trim(line.substr(7));

        // skip kind keywords
        for (const char* kw : {"class ", "struct ", "enum ", "protocol ", "func ", "var ", "typealias "}) {
            if (rest.rfind(kw, 0) == 0) {
                rest = trim(rest.substr(strlen(kw)));
                break;
            }
        }
        // take first identifier / dotted path
        size_t end = rest.find_first_of(" \t{(");
        if (end != std::string::npos) rest = rest.substr(0, end);
        rest = trim(rest);
        if (!rest.empty()) result.push_back(rest);
    }
    return result;
}

fs::path SwiftPlugin::resolve_import(const std::string& imp,
                                     const fs::path& cur_dir,
                                     const fs::path& root) const {
    if (imp.empty()) return {};

    // Module name → Sources/Module/... or Module.swift
    std::string name = imp;
    auto dot = name.find('.');
    if (dot != std::string::npos) name = name.substr(0, dot);

    std::vector<fs::path> candidates;
    candidates.push_back(root / (name + ".swift"));
    candidates.push_back(cur_dir / (name + ".swift"));
    candidates.push_back(root / "Sources" / name / (name + ".swift"));
    candidates.push_back(root / "Source" / (name + ".swift"));
    candidates.push_back(root / name / (name + ".swift"));

    std::error_code ec;
    for (const auto& c : candidates) {
        if (fs::is_regular_file(c, ec)) return c;
    }
    // any .swift under Sources/Name
    fs::path dir = root / "Sources" / name;
    if (fs::is_directory(dir, ec)) {
        for (auto it = fs::directory_iterator(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
            if (it->path().extension() == ".swift")
                return it->path();
        }
    }
    return {};
}

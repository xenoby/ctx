#include "ignore.hpp"
#include "utils.hpp"

std::vector<IgnorePattern> load_ignore_file(const fs::path& path) {
    std::vector<IgnorePattern> result;
    std::ifstream in(path);
    if (!in) return result;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        IgnorePattern pat;
        pat.raw = line;
        if (!line.empty() && line.back() == '/') {
            pat.is_dir = true;
            pat.raw = line.substr(0, line.size() - 1);
        } else {
            pat.has_glob = (line.find('*') != std::string::npos);
        }
        result.push_back(pat);
    }
    return result;
}

bool is_default_excluded(const fs::path& rel) {
    static const std::vector<std::string> dirs = {
        ".git", ".idea", ".vs", ".vscode", ".hg", ".svn",
        "node_modules", "dist", "build", "out", ".next", ".nuxt",
        "coverage", ".turbo", ".cache",
        "__pycache__", "venv", ".venv", "env", ".env",
        ".pytest_cache", ".mypy_cache", ".tox", ".ruff_cache",
        "target",
        ".ctx",
    };

    for (const auto& part : rel) {
        std::string p = part.string();
        for (const auto& d : dirs) {
            if (p == d) return true;
        }
        if (p.rfind("cmake-build", 0) == 0) return true;
    }
    return false;
}

static bool glob_match(const std::string& pattern, const std::string& text) {
    size_t star = pattern.find('*');
    if (star == std::string::npos) return pattern == text;
    std::string prefix = pattern.substr(0, star);
    std::string suffix = pattern.substr(star + 1);
    if (text.size() < prefix.size() + suffix.size()) return false;
    if (text.compare(0, prefix.size(), prefix) != 0) return false;
    if (text.compare(text.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
    return true;
}

bool is_ignored(const fs::path& rel, const std::vector<IgnorePattern>& patterns) {
    for (const auto& pat : patterns) {
        if (pat.is_dir) {
            for (const auto& part : rel) {
                if (part.string() == pat.raw) return true;
            }
        } else if (pat.has_glob) {
            if (glob_match(pat.raw, rel.filename().string())) return true;
            for (const auto& part : rel) {
                if (glob_match(pat.raw, part.string())) return true;
            }
        } else {
            if (rel.filename().string() == pat.raw) return true;
            for (const auto& part : rel) {
                if (part.string() == pat.raw) return true;
            }
        }
    }
    return false;
}

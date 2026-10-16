#include "java_plugin.hpp"
#include "../core/utils.hpp"

bool JavaPlugin::supports(const fs::path& path) const {
    return to_lower(path.extension().string()) == ".java";
}

bool JavaPlugin::is_interface_file(const fs::path& /*path*/) const {
    return false;
}

std::vector<std::string> JavaPlugin::parse_imports(const std::string& content) const {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string raw;

    while (std::getline(stream, raw)) {
        std::string line = trim(normalize_ws(raw));
        if (line.empty() || line.rfind("//", 0) == 0) continue;

        // import foo.bar.Baz;
        // import static foo.bar.Baz.*;
        if (line.rfind("import ", 0) != 0) continue;
        std::string rest = trim(line.substr(7));
        if (rest.rfind("static ", 0) == 0)
            rest = trim(rest.substr(7));

        size_t semi = rest.find(';');
        if (semi != std::string::npos) rest = rest.substr(0, semi);
        // drop wildcard
        if (!rest.empty() && rest.back() == '*') {
            auto dot = rest.rfind('.');
            if (dot != std::string::npos) rest = rest.substr(0, dot);
            else rest.clear();
        }
        rest = trim(rest);
        if (!rest.empty()) result.push_back(rest);
    }
    return result;
}

fs::path JavaPlugin::resolve_import(const std::string& imp,
                                    const fs::path& cur_dir,
                                    const fs::path& root) const {
    if (imp.empty()) return {};

    std::string path_str = imp;
    std::replace(path_str.begin(), path_str.end(), '.', '/');

    std::vector<fs::path> bases = {
        root,
        root / "src" / "main" / "java",
        root / "src",
        cur_dir
    };

    std::error_code ec;
    for (const auto& base : bases) {
        fs::path c = base / (path_str + ".java");
        if (fs::is_regular_file(c, ec)) return c;
    }
    return {};
}

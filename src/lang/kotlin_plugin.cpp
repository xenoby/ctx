#include "kotlin_plugin.hpp"
#include "../core/utils.hpp"

bool KotlinPlugin::supports(const fs::path& path) const {
    std::string ext = to_lower(path.extension().string());
    return ext == ".kt" || ext == ".kts";
}

bool KotlinPlugin::is_interface_file(const fs::path& /*path*/) const {
    return false;
}

std::vector<std::string> KotlinPlugin::parse_imports(const std::string& content) const {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string raw;

    while (std::getline(stream, raw)) {
        std::string line = trim(normalize_ws(raw));
        if (line.empty() || line.rfind("//", 0) == 0) continue;

        // import foo.bar.Baz
        // import foo.bar.Baz as Q
        if (line.rfind("import ", 0) != 0) continue;
        std::string rest = trim(line.substr(7));
        size_t as_pos = rest.find(" as ");
        if (as_pos != std::string::npos) rest = trim(rest.substr(0, as_pos));
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

fs::path KotlinPlugin::resolve_import(const std::string& imp,
                                      const fs::path& cur_dir,
                                      const fs::path& root) const {
    if (imp.empty()) return {};

    std::string path_str = imp;
    std::replace(path_str.begin(), path_str.end(), '.', '/');

    std::vector<fs::path> bases = {
        root,
        root / "src" / "main" / "kotlin",
        root / "src" / "main" / "java",
        root / "src",
        cur_dir
    };

    std::error_code ec;
    for (const auto& base : bases) {
        for (const char* ext : {".kt", ".kts"}) {
            fs::path c = base / (path_str + ext);
            if (fs::is_regular_file(c, ec)) return c;
        }
    }
    return {};
}

#include "csharp_plugin.hpp"
#include "../core/utils.hpp"

bool CSharpPlugin::supports(const fs::path& path) const {
    std::string ext = to_lower(path.extension().string());
    return ext == ".cs";
}

bool CSharpPlugin::is_interface_file(const fs::path& /*path*/) const {
    return false;
}

std::vector<std::string> CSharpPlugin::parse_imports(const std::string& content) const {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string raw;

    while (std::getline(stream, raw)) {
        std::string line = trim(normalize_ws(raw));
        if (line.empty() || line.rfind("//", 0) == 0) continue;

        // using System.Text;
        // using static System.Math;
        // using X = System.Text;
        // global using ...
        std::string work = line;
        if (work.rfind("global ", 0) == 0)
            work = trim(work.substr(7));
        if (work.rfind("using ", 0) != 0) continue;

        std::string rest = trim(work.substr(6));
        if (rest.rfind("static ", 0) == 0)
            rest = trim(rest.substr(7));

        size_t semi = rest.find(';');
        if (semi != std::string::npos) rest = rest.substr(0, semi);

        size_t eq = rest.find('=');
        if (eq != std::string::npos)
            rest = trim(rest.substr(eq + 1));

        rest = trim(rest);
        if (!rest.empty()) result.push_back(rest);
    }
    return result;
}

fs::path CSharpPlugin::resolve_import(const std::string& imp,
                                      const fs::path& cur_dir,
                                      const fs::path& root) const {
    if (imp.empty()) return {};

    // Namespace → path heuristic: Foo.Bar.Baz → Foo/Bar/Baz.cs
    std::string path_str = imp;
    std::replace(path_str.begin(), path_str.end(), '.', '/');

    std::vector<fs::path> candidates;
    candidates.push_back(root / (path_str + ".cs"));
    candidates.push_back(cur_dir / (path_str + ".cs"));
    // last segment as file in folder
    auto pos = path_str.rfind('/');
    if (pos != std::string::npos) {
        candidates.push_back(root / path_str.substr(0, pos) / (path_str.substr(pos + 1) + ".cs"));
    }

    std::error_code ec;
    for (const auto& c : candidates) {
        if (fs::is_regular_file(c, ec)) return c;
    }
    return {};
}

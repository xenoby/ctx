#include "cpp_plugin.hpp"
#include "../core/utils.hpp"

bool CppPlugin::supports(const fs::path& path) const {
    std::string ext = to_lower(path.extension().string());
    return ext == ".h" || ext == ".hpp" || ext == ".hxx" ||
           ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".c";
}

bool CppPlugin::is_interface_file(const fs::path& path) const {
    std::string ext = to_lower(path.extension().string());
    return ext == ".h" || ext == ".hpp" || ext == ".hxx";
}

std::vector<std::string> CppPlugin::parse_imports(const std::string& content) const {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        size_t pos = line.find_first_not_of(" \t");
        if (pos == std::string::npos || line[pos] != '#') continue;

        std::string rest = line.substr(pos + 1);
        size_t inc = rest.find("include");
        if (inc == std::string::npos) continue;
        if (rest.substr(0, inc).find_first_not_of(" \t") != std::string::npos) continue;

        size_t q1 = rest.find('"', inc);
        if (q1 == std::string::npos) continue;
        size_t q2 = rest.find('"', q1 + 1);
        if (q2 == std::string::npos) continue;

        result.push_back(rest.substr(q1 + 1, q2 - q1 - 1));
    }
    return result;
}

fs::path CppPlugin::resolve_import(const std::string& imp,
                                   const fs::path& cur_dir,
                                   const fs::path& root) const {
    std::vector<fs::path> candidates;
    candidates.push_back(cur_dir / imp);
    candidates.push_back(root / imp);

    if (fs::path(imp).extension().empty()) {
        for (const char* ext : {".h", ".hpp", ".hxx"}) {
            candidates.push_back(cur_dir / (imp + ext));
            candidates.push_back(root / (imp + ext));
        }
    }

    std::error_code ec;
    for (const auto& c : candidates) {
        if (fs::is_regular_file(c, ec)) return c;
    }
    return {};
}

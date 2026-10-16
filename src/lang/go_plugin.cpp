#include "go_plugin.hpp"
#include "../core/utils.hpp"

bool GoPlugin::supports(const fs::path& path) const {
    return to_lower(path.extension().string()) == ".go";
}

bool GoPlugin::is_interface_file(const fs::path& /*path*/) const {
    return false;
}

std::vector<std::string> GoPlugin::parse_imports(const std::string& content) const {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string raw;
    bool in_block = false;

    while (std::getline(stream, raw)) {
        std::string line = trim(normalize_ws(raw));
        if (line.empty() || line.rfind("//", 0) == 0) continue;

        if (!in_block && line.rfind("import ", 0) == 0) {
            std::string rest = trim(line.substr(6));
            if (!rest.empty() && rest[0] == '(') {
                in_block = true;
                continue;
            }
            // import "fmt"  or  import alias "fmt"
            size_t q1 = rest.find('"');
            if (q1 == std::string::npos) continue;
            size_t q2 = rest.find('"', q1 + 1);
            if (q2 == std::string::npos) continue;
            result.push_back(rest.substr(q1 + 1, q2 - q1 - 1));
        } else if (in_block) {
            if (line.find(')') != std::string::npos) {
                in_block = false;
                continue;
            }
            size_t q1 = line.find('"');
            if (q1 == std::string::npos) continue;
            size_t q2 = line.find('"', q1 + 1);
            if (q2 == std::string::npos) continue;
            result.push_back(line.substr(q1 + 1, q2 - q1 - 1));
        }
    }
    return result;
}

fs::path GoPlugin::resolve_import(const std::string& imp,
                                  const fs::path& cur_dir,
                                  const fs::path& root) const {
    if (imp.empty()) return {};

    // Only resolve local / relative-looking paths and same-module paths.
    // Skip standard library and remote modules (no dot or known std).
    bool looks_local = imp.find('.') != std::string::npos
                    || imp.rfind("./", 0) == 0
                    || imp.rfind("../", 0) == 0;

    // Also try package path relative to root (project-internal)
    std::vector<fs::path> candidates;

    if (imp.rfind("./", 0) == 0 || imp.rfind("../", 0) == 0) {
        candidates.push_back(cur_dir / imp);
    }

    // package path: example.com/mod/pkg → try under root
    std::string path_str = imp;
    candidates.push_back(root / path_str);
    candidates.push_back(cur_dir / path_str);

    // Go packages are directories; pick any .go file as anchor is hard —
    // try common entry names
    std::vector<fs::path> expanded;
    for (const auto& c : candidates) {
        expanded.push_back(c);
        if (fs::path(c).extension().empty()) {
            expanded.push_back(c / "doc.go");
            // first .go in dir — skip heavy scan; just doc.go / package root
        }
    }

    std::error_code ec;
    for (const auto& c : expanded) {
        if (fs::is_regular_file(c, ec)) return c;
        if (fs::is_directory(c, ec)) {
            // return directory marker as path to a synthetic "package" —
            // try finding any .go
            for (auto it = fs::directory_iterator(c, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
                if (it->path().extension() == ".go")
                    return it->path();
            }
        }
    }

    if (!looks_local) return {}; // stdlib / remote
    return {};
}

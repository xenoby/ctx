#include "rust_plugin.hpp"
#include "../core/utils.hpp"

bool RustPlugin::supports(const fs::path& path) const {
    return to_lower(path.extension().string()) == ".rs";
}

bool RustPlugin::is_interface_file(const fs::path& /*path*/) const {
    // Rust has no separate header; lib.rs / mod.rs act as module roots
    return false;
}

std::vector<std::string> RustPlugin::parse_imports(const std::string& content) const {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string raw;

    while (std::getline(stream, raw)) {
        std::string line = trim(normalize_ws(raw));
        if (line.empty() || line.rfind("//", 0) == 0) continue;

        // use foo::bar::{...};
        // use foo::bar as baz;
        // use foo::bar;
        if (line.rfind("use ", 0) == 0) {
            std::string rest = trim(line.substr(4));
            size_t semi = rest.find(';');
            if (semi != std::string::npos) rest = rest.substr(0, semi);
            size_t as_pos = rest.find(" as ");
            if (as_pos != std::string::npos) rest = trim(rest.substr(0, as_pos));
            // strip brace groups: foo::bar::{A, B} → foo::bar
            size_t brace = rest.find("::{");
            if (brace != std::string::npos) rest = rest.substr(0, brace);
            size_t brace2 = rest.find('{');
            if (brace2 != std::string::npos) rest = trim(rest.substr(0, brace2));
            if (!rest.empty()) result.push_back(rest);
        }
        // mod foo;  or  mod foo {
        else if (line.rfind("mod ", 0) == 0) {
            std::string rest = trim(line.substr(4));
            size_t end = rest.find_first_of(";{");
            if (end != std::string::npos) rest = trim(rest.substr(0, end));
            if (!rest.empty()) result.push_back(rest);
        }
    }
    return result;
}

fs::path RustPlugin::resolve_import(const std::string& imp,
                                    const fs::path& cur_dir,
                                    const fs::path& root) const {
    if (imp.empty()) return {};

    // crate:: / super:: / self:: — relative to project or current
    std::string path_str = imp;
    // drop leading crate::
    if (path_str.rfind("crate::", 0) == 0)
        path_str = path_str.substr(7);

    std::replace(path_str.begin(), path_str.end(), ':', '/');
    // collapse //
    while (path_str.find("//") != std::string::npos) {
        auto p = path_str.find("//");
        path_str.replace(p, 2, "/");
    }

    std::vector<fs::path> candidates;
    auto push = [&](const fs::path& base) {
        candidates.push_back(base / (path_str + ".rs"));
        candidates.push_back(base / path_str / "mod.rs");
        candidates.push_back(base / path_str / "lib.rs");
    };

    push(cur_dir);
    push(root);
    push(root / "src");

    // super:: — parent of current
    if (imp.rfind("super::", 0) == 0) {
        std::string rest = imp.substr(7);
        std::replace(rest.begin(), rest.end(), ':', '/');
        candidates.push_back(cur_dir.parent_path() / (rest + ".rs"));
        candidates.push_back(cur_dir.parent_path() / rest / "mod.rs");
    }

    std::error_code ec;
    for (const auto& c : candidates) {
        if (fs::is_regular_file(c, ec)) return c;
    }
    return {};
}

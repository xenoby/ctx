#include "python_plugin.hpp"
#include "../core/utils.hpp"

bool PythonPlugin::supports(const fs::path& path) const {
    std::string ext = to_lower(path.extension().string());
    return ext == ".py" || ext == ".pyi";
}

bool PythonPlugin::is_interface_file(const fs::path& path) const {
    return to_lower(path.extension().string()) == ".pyi";
}

std::vector<std::string> PythonPlugin::parse_imports(const std::string& content) const {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string raw_line;

    while (std::getline(stream, raw_line)) {
        std::string line = normalize_ws(raw_line);
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        if (trimmed.rfind("import", 0) == 0) {
            size_t after = 6;
            if (after >= trimmed.size() || trimmed[after] != ' ') continue;
            while (after < trimmed.size() && trimmed[after] == ' ') after++;

            std::string rest = trimmed.substr(after);
            size_t comment = rest.find('#');
            if (comment != std::string::npos) rest = rest.substr(0, comment);

            for (auto part : split(rest, ',')) {
                part = trim(part);
                size_t as_pos = part.find(" as ");
                if (as_pos != std::string::npos) part = trim(part.substr(0, as_pos));
                if (!part.empty()) result.push_back(part);
            }
        }
        else if (trimmed.rfind("from", 0) == 0) {
            size_t after = 4;
            if (after >= trimmed.size() || trimmed[after] != ' ') continue;
            while (after < trimmed.size() && trimmed[after] == ' ') after++;

            std::string rest = trimmed.substr(after);
            size_t comment = rest.find('#');
            if (comment != std::string::npos) rest = rest.substr(0, comment);

            size_t imp_pos = rest.find(" import ");
            if (imp_pos == std::string::npos) continue;

            std::string module = trim(rest.substr(0, imp_pos));
            if (!module.empty()) result.push_back(module);
        }
    }
    return result;
}

fs::path PythonPlugin::resolve_import(const std::string& module,
                                      const fs::path& cur_dir,
                                      const fs::path& root) const {
    if (module.empty()) return {};

    size_t dots = 0;
    while (dots < module.size() && module[dots] == '.') dots++;

    fs::path base;
    std::string rest;

    if (dots > 0) {
        rest = module.substr(dots);
        base = cur_dir;
        for (size_t i = 1; i < dots; ++i) base = base.parent_path();
    } else {
        rest = module;
    }

    std::string path_str = rest;
    std::replace(path_str.begin(), path_str.end(), '.', '/');

    std::vector<fs::path> candidates;
    if (dots > 0) {
        if (path_str.empty()) {
            candidates.push_back(base / "__init__.py");
        } else {
            candidates.push_back(base / (path_str + ".py"));
            candidates.push_back(base / path_str / "__init__.py");
        }
    } else {
        candidates.push_back(root / (path_str + ".py"));
        candidates.push_back(root / path_str / "__init__.py");
        candidates.push_back(cur_dir / (path_str + ".py"));
        candidates.push_back(cur_dir / path_str / "__init__.py");
    }

    std::error_code ec;
    for (const auto& c : candidates) {
        if (fs::is_regular_file(c, ec)) return c;
    }
    return {};
}

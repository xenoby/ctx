#include "js_plugin.hpp"
#include "../core/utils.hpp"

bool JsPlugin::supports(const fs::path& path) const {
    std::string ext = to_lower(path.extension().string());
    std::string name = to_lower(path.filename().string());
    if (ends_with(name, ".d.ts")) return true;
    return ext == ".ts" || ext == ".tsx" || ext == ".mts" || ext == ".cts" ||
           ext == ".js" || ext == ".jsx" || ext == ".mjs" || ext == ".cjs";
}

bool JsPlugin::is_interface_file(const fs::path& path) const {
    return ends_with(to_lower(path.filename().string()), ".d.ts");
}

static void extract_quoted_after(const std::string& line, const std::string& kw,
                                 std::vector<std::string>& out) {
    size_t pos = 0;
    while ((pos = line.find(kw, pos)) != std::string::npos) {
        pos += kw.size();
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
        if (pos >= line.size()) break;

        char quote = line[pos];
        if (quote != '\'' && quote != '"') { pos++; continue; }

        size_t end = line.find(quote, pos + 1);
        if (end == std::string::npos) break;

        std::string p = line.substr(pos + 1, end - pos - 1);
        if (!p.empty()) out.push_back(p);
        pos = end + 1;
    }
}

std::vector<std::string> JsPlugin::parse_imports(const std::string& content) const {
    std::vector<std::string> result;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        extract_quoted_after(line, "from", result);
        extract_quoted_after(line, "require(", result);
        extract_quoted_after(line, "import(", result);
    }
    return result;
}

fs::path JsPlugin::resolve_import(const std::string& imp,
                                  const fs::path& cur_dir,
                                  const fs::path& /*root*/) const {
    if (imp.empty() || imp[0] != '.') return {};

    std::vector<fs::path> candidates;
    candidates.push_back(cur_dir / imp);
    for (const char* ext : {".ts", ".tsx", ".d.ts", ".js", ".jsx", ".mjs", ".cjs"}) {
        candidates.push_back(cur_dir / (imp + ext));
    }
    for (const char* ext : {".ts", ".tsx", ".d.ts", ".js", ".jsx"}) {
        candidates.push_back(cur_dir / imp / ("index" + std::string(ext)));
    }

    std::error_code ec;
    for (const auto& c : candidates) {
        if (fs::is_regular_file(c, ec)) return c;
    }
    return {};
}

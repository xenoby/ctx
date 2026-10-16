#include "scanner.hpp"
#include "utils.hpp"
#include <iostream>
#include <algorithm>

ScanResult scan_project(const fs::path& root,
                        const PluginRegistry& registry,
                        const std::vector<IgnorePattern>& ignore_patterns) {
    ScanResult result;

    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {
        std::error_code ec;
        if (!it->is_regular_file(ec)) continue;

        fs::path abs = it->path();
        fs::path rel = fs::relative(abs, root, ec);
        if (ec) continue;

        if (!registry.is_supported(abs)) continue;
        if (is_default_excluded(rel)) continue;
        if (is_ignored(rel, ignore_patterns)) continue;

        auto hr = hash_file(abs);
        if (!hr.ok) {
            std::cerr << "[WARN] cannot read: " << abs << "\n";
            continue;
        }

        FileInfo fi;
        fi.abs = abs;
        fi.rel = rel.generic_string();
        fi.hash = fnv1a_hex(hr.hash);
        fi.size = hr.size;
        fi.binary = hr.binary;
        fi.lang = registry.detect_lang(abs);
        result.files.push_back(std::move(fi));

        result.lang_counts[lang_to_string(result.files.back().lang)]++;
    }

    std::sort(result.files.begin(), result.files.end(),
              [](const FileInfo& a, const FileInfo& b) { return a.rel < b.rel; });

    return result;
}

std::unordered_set<std::string> collect_deps(const fs::path& entry,
                                             const fs::path& root,
                                             const PluginRegistry& registry) {
    std::unordered_set<std::string> visited;
    std::vector<fs::path> stack;

    std::error_code ec;
    fs::path entry_abs = fs::weakly_canonical(fs::absolute(entry), ec);
    if (ec) entry_abs = fs::absolute(entry);
    stack.push_back(entry_abs);

    while (!stack.empty()) {
        fs::path current = stack.back();
        stack.pop_back();

        std::string cur_key = current.string();
        if (visited.count(cur_key)) continue;
        visited.insert(cur_key);

        const ILanguagePlugin* plugin = registry.find(current);
        if (!plugin) continue;

        std::string content;
        if (!read_file_content(current, content)) continue;

        for (const auto& imp : plugin->parse_imports(content)) {
            fs::path resolved = plugin->resolve_import(imp, current.parent_path(), root);
            if (resolved.empty()) continue;

            std::error_code ec2;
            fs::path canon = fs::weakly_canonical(resolved, ec2);
            if (ec2) canon = resolved;

            if (!visited.count(canon.string()))
                stack.push_back(canon);
        }
    }
    return visited;
}

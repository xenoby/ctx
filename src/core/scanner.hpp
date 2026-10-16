#pragma once

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "file_info.hpp"
#include "ignore.hpp"
#include "../lang/plugin_registry.hpp"

namespace fs = std::filesystem;

struct ScanResult {
    std::vector<FileInfo> files;
    std::unordered_map<std::string, int> lang_counts;
};

ScanResult scan_project(const fs::path& root,
                        const PluginRegistry& registry,
                        const std::vector<IgnorePattern>& ignore_patterns);

/** Collect transitive dependencies starting from entry file */
std::unordered_set<std::string> collect_deps(const fs::path& entry,
                                             const fs::path& root,
                                             const PluginRegistry& registry);

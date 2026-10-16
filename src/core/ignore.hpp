#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct IgnorePattern {
    std::string raw;
    bool is_dir = false;
    bool has_glob = false;
};

std::vector<IgnorePattern> load_ignore_file(const fs::path& path);
bool is_default_excluded(const fs::path& rel);
bool is_ignored(const fs::path& rel, const std::vector<IgnorePattern>& patterns);

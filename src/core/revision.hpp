#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct RevisionInfo {
    int current = 0;
    int parent = 0;
};

fs::path ctx_dir(const fs::path& root);
bool is_initialized(const fs::path& root);

RevisionInfo read_revision(const fs::path& root);
void write_revision(const fs::path& root, int rev);

void append_history(const fs::path& root,
                    int rev, int parent,
                    const std::vector<std::string>& modified,
                    const std::vector<std::string>& added,
                    const std::vector<std::string>& deleted,
                    const std::string& reason = "");

/** Read history.jsonl lines (newest last). */
std::vector<std::string> read_history_lines(const fs::path& root);

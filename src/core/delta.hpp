#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "file_info.hpp"

struct DeltaResult {
    std::vector<OutputFile> files_to_write;
    std::vector<std::string> deleted;
    std::vector<std::string> added;
    std::vector<std::string> modified;
    bool have_prev = false;
};

/** Parse hash_manifest from a previous context XML file */
std::unordered_map<std::string, std::string> parse_manifest_xml(const std::filesystem::path& path);

/** Simple line-based manifest stored in .ctx/manifest (path<TAB>hash) */
std::unordered_map<std::string, std::string> load_manifest(const std::filesystem::path& path);
void save_manifest(const std::filesystem::path& path,
                   const std::vector<FileInfo>& files);

DeltaResult compute_delta(const std::vector<FileInfo*>& candidates,
                          const std::vector<FileInfo>& all_files,
                          const std::unordered_map<std::string, std::string>& prev_manifest,
                          bool have_prev);

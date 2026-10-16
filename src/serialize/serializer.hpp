#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "../core/file_info.hpp"

struct ContextData {
    std::string mode;               // "full" | "delta"
    int revision = 0;
    int parent_revision = 0;
    int base_revision = 0;
    std::string root;
    size_t total_files = 0;
    size_t included_files = 0;
    size_t deleted_files = 0;
    uintmax_t total_size_kb = 0;
    std::string hash_algorithm = "fnv1a-64";
    std::string entry;
    bool headers_only = false;

    std::unordered_map<std::string, int> languages;
    std::vector<FileInfo> all_files;
    std::vector<OutputFile> files_to_write;
    std::vector<std::string> deleted;
    std::vector<std::string> added;
    std::vector<std::string> modified;
};

class ISerializer {
public:
    virtual ~ISerializer() = default;
    virtual bool write(const ContextData& data, const std::string& path) = 0;
};

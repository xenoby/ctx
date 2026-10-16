#pragma once

#include <filesystem>
#include <string>
#include <cstdint>
#include "types.hpp"

namespace fs = std::filesystem;

struct FileInfo {
    fs::path abs;
    std::string rel;          // relative path with generic separators
    std::string hash;         // fnv1a-64 hex
    uintmax_t size = 0;
    bool binary = false;
    Lang lang = Lang::Unknown;
};

struct OutputFile {
    const FileInfo* info = nullptr;
    std::string status;       // "full" | "new" | "modified"
    /** If set, serializer uses this instead of reading from disk / blob. */
    std::string content_override;
};

struct FileHashResult {
    uint64_t hash = 0;
    uintmax_t size = 0;
    bool binary = false;
    bool ok = false;
};

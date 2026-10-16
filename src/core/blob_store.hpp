#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include "file_info.hpp"

namespace fs = std::filesystem;

/**
 * Content-addressable blob store under .ctx/objects/<hh>/<hash>
 * Hash is fnv1a-64 hex of file content (same as FileInfo::hash).
 */
class BlobStore {
public:
    explicit BlobStore(const fs::path& root);

    /** Store file content if not already present. Returns hash. */
    std::string store_file(const fs::path& abs_path, const std::string& known_hash = "");

    /** Store raw content. Returns hash. */
    std::string store_content(const std::string& content, const std::string& known_hash = "");

    /** Load blob by hash. Returns empty string on failure. */
    std::string load(const std::string& hash) const;

    bool exists(const std::string& hash) const;

    fs::path blob_path(const std::string& hash) const;

    /** Store all files from scan that are new/modified; skip unchanged. */
    void store_changed(const std::vector<FileInfo>& all_files,
                       const std::vector<std::string>& added,
                       const std::vector<std::string>& modified);

private:
    fs::path root_;
    fs::path objects_dir_;
};

/** Per-revision manifest: path -> hash at that revision */
using RevManifest = std::unordered_map<std::string, std::string>;

void save_rev_manifest(const fs::path& root, int rev, const std::vector<FileInfo>& files);
RevManifest load_rev_manifest(const fs::path& root, int rev);
bool rev_manifest_exists(const fs::path& root, int rev);
